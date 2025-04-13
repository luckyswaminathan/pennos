#include "src/pennfat/fat.h"
#include "src/pennfat/fat_utils.h"

#include <stdint.h>
#include <stdbool.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// should be a value storable in a uint16_t
// and less than GLOBAL_FD_TABLE_ENTRY_NOT_FOUND_SENTINEL
// (so at most max(uint16_t) - 1)
#define GLOBAL_FD_TABLE_SIZE 4096
#define GLOBAL_FD_TABLE_ENTRY_NOT_FOUND_SENTINEL 0xFFFF
#define MIN_FILENAME_SIZE 1
#define MAX_FILENAME_SIZE 31
#define FAT_END_OF_FILE 0xFFFF

global_fd_entry global_fd_table[GLOBAL_FD_TABLE_SIZE] = {0};
fat16_fs fs = {
    .fat = NULL,
    .fat_size = 0,
    .block_size = 0,
    .blocks_in_fat = 0,
    .fd = -1,
    .block_buf = NULL};

int min(int a, int b)
{
    if (a < b)
        return a;
    return b;
}

bool is_mounted(void)
{
    return fs.fd != -1;
}

int mount(char *fs_name)
{
    if (is_mounted())
    {
        return EMOUNT_ALREADY_MOUNTED;
    }

    int fs_fd = open(fs_name, O_RDWR);
    if (fs_fd == -1)
    {
        return EMOUNT_OPEN_FAILED;
    }

    // compute the fat size
    uint16_t first_entry;
    if (read(fs_fd, &first_entry, 2) == -1)
    {
        return EMOUNT_READ_FAILED;
    }

    uint8_t blocks_in_fat;
    uint16_t block_size;
    if (parse_first_fat_entry(first_entry, &block_size, &blocks_in_fat) == -1)
    {
        return EMOUNT_BAD_FAT_FIRST_ENTRY;
    }

    size_t fat_size = (size_t)blocks_in_fat * block_size;
    uint16_t *fat = mmap(NULL, fat_size, PROT_READ | PROT_WRITE, MAP_SHARED, fs_fd, 0);
    if (fat == MAP_FAILED)
    {
        return EMOUNT_MMAP_FAILED;
    }

    void *block_buf = malloc(block_size);
    if (block_buf == NULL)
    {
        return EMOUNT_MALLOC_FAILED;
    }

    fs = (fat16_fs){
        .fat = fat,
        .fat_size = fat_size,
        .block_size = block_size,
        .blocks_in_fat = blocks_in_fat,
        .fd = fs_fd,
        .block_buf = block_buf};

    // initialize the global fd table with entries for 0, 1, 2
    // as STDIN, STDOUT, and STDERR
    // These are special non-closeable files
    for (int i = 0; i < 3; i++)
    {
        global_fd_table[i].ref_count = 1;
        global_fd_table[i].ptr_to_dir_entry = NULL;
        global_fd_table[i].dir_entry_block_num = 0;
        global_fd_table[i].dir_entry_idx = 0;
        global_fd_table[i].write_locked = 0;
        global_fd_table[i].offset = 0;
    }

    return 0;
}

int unmount(void)
{
    if (!is_mounted())
    {
        return EFS_NOT_MOUNTED;
    }

    // best effort to close all open files
    for (int i = 0; i < GLOBAL_FD_TABLE_SIZE; i++)
    {
        if (global_fd_table[i].ref_count > 0)
        {
            global_fd_table[i].ref_count = 1; // set this
            k_close(i);
        }
    }

    free(fs.block_buf);
    if (munmap(fs.fat, fs.fat_size) == -1)
    {
        return EUNMOUNT_MUNMAP_FAILED;
    }
    if (close(fs.fd) != 0)
    {
        return EUNMOUNT_CLOSE_FAILED;
    }
    fs = (fat16_fs){0};
    fs.fat = NULL; // just to be safe, set the ptr to NULL (in case NULL != 0)
    fs.block_buf = NULL;
    fs.fd = -1;
    return 0;
}

/**
 * Clear a file starting at block. It is expected that block is
 * the first block in the file. If it is not
 *
 * Note that this function does not validate that the blocks at each
 * stage (including the initially passed block) are in bounds. It assumes
 * the FAT is maintained as valid.
 */
void clear_fat_file(uint16_t block)
{
    while (block != FAT_END_OF_FILE)
    {
        uint16_t next_block = fs.fat[block];
        fs.fat[block] = 0;
        block = next_block;
    }
}

/**
 * Finds the first empty block by walking the fat from index 1. Returns the
 * block index if such a block exists and 0 if there is no empty block
 */
uint16_t first_empty_block(void)
{
    // look for the first empty block in the fat
    uint32_t n_fat_entries = fs.fat_size == 65536 ? fs.fat_size / 2 : 65535;
    if (n_fat_entries >= (1 << 16))
    {
        n_fat_entries = FAT_END_OF_FILE;
    }

    for (uint16_t i = 1; i < n_fat_entries; i++)
    {
        if (fs.fat[i] == 0)
        {
            return i;
        }
    }
    return 0;
}

/**
 * Whether the provided string fits thei POSIX filename charset
 */
bool check_filename_charset(const char *str, uint8_t strlen)
{
    for (uint8_t i = 0; i < strlen; i++)
    {
        if (
            !(
                ('a' <= str[i] && str[i] <= 'z') ||
                ('A' <= str[i] && str[i] <= 'Z') ||
                ('0' <= str[i] && str[i] <= '9') ||
                str[i] == '.' ||
                str[i] == '_' ||
                str[i] == '-'))
        {
            return false;
        }
    }
    return true;
}

uint32_t get_blocks_in_data_region(void)
{
    return ((fs.fat_size) / 2) - 1;
}

uint32_t get_byte_offset_of_block(uint16_t block_num)
{
    return fs.fat_size + ((size_t)block_num - 1) * fs.block_size;
}

#define EGET_BLOCK_BLOCK_NUM_0 1
#define EGET_BLOCK_BLOCK_NUM_TOO_HIGH 2
#define EGET_BLOCK_LSEEK_FAILED 4
#define EGET_BLOCK_READ_FAILED 5
#define EGET_BLOCK_TOO_FEW_BYTES_READ 6

/**
 * Get the data inside of a block and store it into the buffer pointed to
 * by data. It is assumed that the memory pointed to by data has sufficient
 * capacity to store a full block (fs.block_size).
 *
 * Returns 0 on success and an error code on error. See the EGET_BLOCK_* error code.
 */
int get_block(uint16_t block_num, void *data)
{
    uint32_t blocks_in_data_region = get_blocks_in_data_region();
    if (block_num < 1)
    {
        return EGET_BLOCK_BLOCK_NUM_0;
    }

    if (block_num > blocks_in_data_region)
    {
        return EGET_BLOCK_BLOCK_NUM_TOO_HIGH;
    }

    if (lseek(fs.fd, get_byte_offset_of_block(block_num), SEEK_SET) < 0)
    {
        return EGET_BLOCK_LSEEK_FAILED;
    }

    ssize_t bytes_read = read(fs.fd, data, fs.block_size);
    if (bytes_read == -1)
    {
        return EGET_BLOCK_READ_FAILED;
    }

    if (bytes_read < fs.block_size)
    {
        return EGET_BLOCK_TOO_FEW_BYTES_READ;
    }

    return 0;
}

#define ENEXT_BLOCK_NUM_BLOCK_NUM_0 1
#define ENEXT_BLOCK_NUM_BLOCK_NUM_TOO_HIGH 2
// TODO: add a return condition for already being at the end file

/**
 * Get the next block number from the FAT table.
 */
int next_block_num(uint16_t block_num, uint16_t *next_block_num)
{
    uint32_t blocks_in_data_region = get_blocks_in_data_region();
    if (block_num < 1)
    {
        return ENEXT_BLOCK_NUM_BLOCK_NUM_0;
    }

    if (block_num > blocks_in_data_region)
    {
        return ENEXT_BLOCK_NUM_BLOCK_NUM_TOO_HIGH;
    }
    *next_block_num = fs.fat[block_num];
    return 0;
}

#define EWRITE_BLOCK_BLOCK_NUM_0 1
#define EWRITE_BLOCK_BLOCK_NUM_TOO_HIGH 2
#define EWRITE_BLOCK_LSEEK_FAILED 4
#define EWRITE_BLOCK_WRITE_FAILED 5
#define EWRITE_BLOCK_TOO_FEW_BYTES_WRITTEN 6

/**
 * A thin wrapper around wrapper that makes it easier to write exactly 1 block of data
 */
int write_block(uint16_t block_num, void *data)
{
    uint32_t blocks_in_data_region = get_blocks_in_data_region();
    if (block_num < 1)
    {
        return EWRITE_BLOCK_BLOCK_NUM_0;
    }

    if (block_num > blocks_in_data_region)
    {
        return EWRITE_BLOCK_BLOCK_NUM_TOO_HIGH;
    }

    if (lseek(fs.fd, get_byte_offset_of_block(block_num), SEEK_SET) < 0)
    {
        return EWRITE_BLOCK_LSEEK_FAILED;
    }

    ssize_t bytes_written = write(fs.fd, data, fs.block_size);
    if (bytes_written == -1)
    {
        return EWRITE_BLOCK_WRITE_FAILED;
    }

    if (bytes_written < fs.block_size)
    {
        return EWRITE_BLOCK_TOO_FEW_BYTES_WRITTEN;
    }

    return 0;
}

#define EFIND_FILE_IN_ROOT_DIR_GET_BLOCK_FAILED -1
#define EFIND_FILE_IN_ROOT_DIR_NEXT_BLOCK_FAILED -2
#define RFIND_FILE_IN_ROOT_DIR_FILE_NOT_FOUND 1
#define RFIND_FILE_IN_ROOT_DIR_FILE_FOUND 0
#define RFIND_FILE_IN_ROOT_DIR_FILE_DELETED 2

/**
 * Find the file with name fname in the root directory of the filesystem. If it is found,
 * mallocs new directory_entry and returns a pointer to it via the ptr_to_dir_entry_buf
 *
 * Returns >= 0 on success (see RFIND_FILE_IN_ROOT_DIR_* return codes) and < 0 on error (see EFIND_FILE_IN_ROOT_DIR_* error codes)
 */
int find_file_in_root_dir(const char *fname, directory_entry *ptr_to_dir_entry, uint16_t *ptr_to_block, uint8_t *ptr_to_dir_entry_idx)
{
    uint16_t block = 1;
    directory_entry *dir_entry_buf = fs.block_buf;
    // n_dir_entry_per_block is at most 4096 / 64 = 64
    uint8_t n_dir_entry_per_block = fs.block_size / sizeof(directory_entry);

    while (true)
    {
        if (get_block(block, dir_entry_buf) != 0)
        {
            return EFIND_FILE_IN_ROOT_DIR_GET_BLOCK_FAILED;
        }

        for (uint8_t i = 0; i < n_dir_entry_per_block; i++)
        {
            directory_entry curr_dir_entry = dir_entry_buf[i];
            if (curr_dir_entry.name[0] == 0)
            {
                return RFIND_FILE_IN_ROOT_DIR_FILE_NOT_FOUND;
            }

            // use strcmp because both fname and the directory entries in the fat should
            // have been checked for proper null termination
            if (strcmp(fname, curr_dir_entry.name) == 0)
            {
                // memcpy into the passed buffer so we're not
                // pointing to the fs.block_buf, which could
                // change
                memcpy(ptr_to_dir_entry, dir_entry_buf + i, sizeof(directory_entry));
                *ptr_to_block = block;
                *ptr_to_dir_entry_idx = i;

                // TODO: the below should never happen
                if (curr_dir_entry.name[0] == 1 || curr_dir_entry.name[0] == 2)
                {
                    return RFIND_FILE_IN_ROOT_DIR_FILE_DELETED;
                }
                return RFIND_FILE_IN_ROOT_DIR_FILE_FOUND;
            }
        }

        if (next_block_num(block, &block) != 0)
        {
            return EFIND_FILE_IN_ROOT_DIR_GET_BLOCK_FAILED;
        }
    };
}

#define RFIND_FILE_IN_GLOBAL_FD_TABLE_NOT_FOUND 1

/**
 * Looks for a file in the global file table matching fname, returning 0 and
 * setting the memory address at ptr_to_fd_idx to the index in the file table if found
 * and returning RFIND_FILE_IN_GLOBAL_FD_TABLE_NOT_FOUND (1) if not found.
 *
 * Note that this function skips files that have 0,1 or 2 as the first byte of their name
 * because these entries are not valid entries (in fact, they've have their name mangled, so
 * they should not match any but an adversarial fname). It also skips files with a ref_count of 0
 */
int find_file_in_global_fd_table(const char *fname, uint16_t *ptr_to_fd_idx)
{
    // Skip the first 3 entries because they are special
    for (uint16_t i = 3; i < GLOBAL_FD_TABLE_SIZE; i++)
    {
        // use strcmp because both fname and the directory entries in the fat/global fd table should
        // have been checked for proper null termination
        if (global_fd_table[i].ref_count == 0 || global_fd_table[i].ptr_to_dir_entry->name[0] < 3)
            continue;
        if (strcmp(global_fd_table[i].ptr_to_dir_entry->name, fname) == 0)
        {
            *ptr_to_fd_idx = i;
            return 0;
        }
    }
    return RFIND_FILE_IN_GLOBAL_FD_TABLE_NOT_FOUND;
}

#define EFIND_EMPTY_SPOT_IN_ROOT_DIR_GET_BLOCK_FAILED -1
#define EFIND_EMPTY_SPOT_IN_ROOT_DIR_NEXT_BLOCK_FAILED -2
#define RFIND_EMPTY_SPOT_IN_ROOT_DIR_DELETED 0
#define RFIND_EMPTY_SPOT_IN_ROOT_DIR_END_ENTRY 1

int find_empty_spot_in_root_dir(uint16_t *ptr_to_block, uint8_t *ptr_to_offset)
{
    uint16_t block = 1;
    directory_entry *dir_entry_buf = fs.block_buf;
    uint8_t n_dir_entry_per_block = fs.block_size / sizeof(directory_entry);
    while (true)
    {
        if (get_block(block, dir_entry_buf) != 0)
        {
            return EFIND_EMPTY_SPOT_IN_ROOT_DIR_GET_BLOCK_FAILED;
        }

        // n_dir_entry_per_block is at most 4096 / 64 = 64
        for (uint8_t i = 0; i < n_dir_entry_per_block; i++)
        {
            directory_entry curr_dir_entry = dir_entry_buf[i];
            if (curr_dir_entry.name[0] > 2)
            { // not a  special entry
                continue;
            }

            *ptr_to_block = block;
            *ptr_to_offset = i;
            if (curr_dir_entry.name[0] == 0)
            {
                return RFIND_EMPTY_SPOT_IN_ROOT_DIR_END_ENTRY;
            }
            if (curr_dir_entry.name[0] == 1 || curr_dir_entry.name[0] == 2)
            {
                return RFIND_EMPTY_SPOT_IN_ROOT_DIR_DELETED;
            }
        }

        if (next_block_num(block, &block) != 0)
        {
            return EFIND_EMPTY_SPOT_IN_ROOT_DIR_NEXT_BLOCK_FAILED;
        }
    };
}

#define EWRITE_ROOT_DIR_ENTRY_GET_BLOCK_FAILED 1
#define EWRITE_ROOT_DIR_ENTRY_NO_EMPTY_BLOCKS 2
#define EWRITE_ROOT_DIR_ENTRY_WRITE_BLOCK_FAILED 3

int write_root_dir_entry(directory_entry *ptr_to_dir_entry, uint16_t block, uint8_t directory_entry_offset)
{
    directory_entry *dir_entry_buf = (directory_entry *)fs.block_buf;
    if (get_block(block, dir_entry_buf) != 0)
    {
        return EWRITE_ROOT_DIR_ENTRY_GET_BLOCK_FAILED;
    }

    // the last entry of the end_block can now store our dir_entry (replacing the directory_entry that represents
    // end of directory)
    dir_entry_buf[directory_entry_offset] = *ptr_to_dir_entry;

    // write the block
    if (write_block(block, dir_entry_buf) != 0)
    {
        return EWRITE_ROOT_DIR_ENTRY_WRITE_BLOCK_FAILED;
    }
    return 0;
}

#define EWRITE_NEW_ROOT_DIR_ENTRY_FIND_EMPTY_SPOT_IN_ROOT_DIR_FAILED 1
#define EWRITE_NEW_ROOT_DIR_ENTRY_WRITE_ROOT_DIR_ENTRY_FAILED 2

int write_new_root_dir_entry(directory_entry *ptr_to_dir_entry, uint16_t *ptr_to_block, uint8_t *ptr_to_dir_entry_idx)
{
    // look for an empty spot in the root directory
    uint16_t block;                 // the block where we found an empty spot
    uint8_t directory_entry_offset; // the offset (in terms of directory_entry entries) where we found the empty spot

    int find_empty_spot_status = find_empty_spot_in_root_dir(&block, &directory_entry_offset);
    if (find_empty_spot_status < 0)
    {
        return EWRITE_NEW_ROOT_DIR_ENTRY_FIND_EMPTY_SPOT_IN_ROOT_DIR_FAILED;
    }

    bool is_last_in_block = directory_entry_offset == (fs.block_size / sizeof(directory_entry) - 1);
    if (find_empty_spot_status == RFIND_EMPTY_SPOT_IN_ROOT_DIR_END_ENTRY && !is_last_in_block)
    {
        // case where we need to create an entry for the end of the block
        directory_entry empty_dir_entry = {0};
        write_root_dir_entry(&empty_dir_entry, block, directory_entry_offset + 1);
    }
    else if (find_empty_spot_status == RFIND_EMPTY_SPOT_IN_ROOT_DIR_END_ENTRY && is_last_in_block)
    {
        // case where we need to allocate a new block for the directory

        uint16_t empty_block = first_empty_block();
        if (empty_block == 0)
        {
            return EWRITE_ROOT_DIR_ENTRY_NO_EMPTY_BLOCKS;
        }

        // zero out the new block
        // this makes the first directory_entry in the new block
        // the end directory entry
        memset(fs.block_buf, 0, fs.block_size);
        if (write_block(empty_block, fs.block_buf) != 0)
        {
            return EWRITE_ROOT_DIR_ENTRY_WRITE_BLOCK_FAILED;
        }

        // add the new block to the FAT
        fs.fat[block] = empty_block;
        fs.fat[empty_block] = FAT_END_OF_FILE;
    }

    if (write_root_dir_entry(ptr_to_dir_entry, block, directory_entry_offset) != 0)
    {
        return EWRITE_NEW_ROOT_DIR_ENTRY_WRITE_ROOT_DIR_ENTRY_FAILED;
    }
    *ptr_to_block = block;
    *ptr_to_dir_entry_idx = directory_entry_offset;
    return 0;
}

uint16_t find_empty_fd()
{
    // find an empty fd_idx // TODO: refactor into its own function
    uint16_t fd_idx = GLOBAL_FD_TABLE_ENTRY_NOT_FOUND_SENTINEL;
    // Skip the first 3 entries because they are special
    for (uint16_t i = 3; i < GLOBAL_FD_TABLE_SIZE; i++)
    {
        if (global_fd_table[i].ref_count == 0)
        {
            fd_idx = i;
            break;
        }
    }
    return fd_idx;
}

bool is_valid_filename(const char *fname)
{
    uint8_t strlen = strnlen(fname, MAX_FILENAME_SIZE);
    if (strlen < 1)
    {
        return false;
    }
    if (fname[strlen] != '\0')
    {
        // the string must be longer than MAX_FILENAME_SIZE
        return false;
    }
    if (!check_filename_charset(fname, strlen))
    {
        return false;
    }
    return true;
}

int k_open(const char *fname, int mode)
{
    if (!is_mounted())
    {
        return EFS_NOT_MOUNTED;
    }

    if (!is_valid_filename(fname))
    {
        return EK_OPEN_INVALID_FILENAME;
    }
    // NOTE: because of the above validation, you can never open a deleted file
    // or end dir entry since they start with 0, 1, or 2 which are not part of the
    // valid filename charset

    uint16_t fd_idx;
    int find_file_in_global_fd_table_status = find_file_in_global_fd_table(fname, &fd_idx);

    // Already found the file in the global_fd_table
    if (find_file_in_global_fd_table_status == 0)
    {
        global_fd_entry *fd_entry = &global_fd_table[fd_idx];

        // we already have an entry in the global file table
        if (fd_entry->write_locked && (mode == F_WRITE || mode == F_APPEND))
        {
            return EK_OPEN_ALREADY_WRITE_LOCKED;
        }

        uint8_t perm = fd_entry->ptr_to_dir_entry->perm;
        if (
            perm == P_NO_FILE_PERMISSION ||
            (perm == P_WRITE_ONLY_FILE_PERMISSION && mode == F_READ) || // NOTE: F_WRITE also allows reading, but we only gate this on the f_write function
            ((perm == P_READ_ONLY_FILE_PERMISSION || perm == P_READ_AND_EXECUTABLE_FILE_PERMISSION) && (mode == F_WRITE || mode == F_APPEND)))
        {
            return EK_OPEN_WRONG_PERMISSIONS;
        }
    }
    else
    {

        // Otherwise: the fd_idx was not found so we need to make a new entry
        // in the global file table
        fd_idx = find_empty_fd();
        if (fd_idx == GLOBAL_FD_TABLE_ENTRY_NOT_FOUND_SENTINEL)
        {
            return EK_OPEN_GLOBAL_FD_TABLE_FULL;
        }

        // find or create a dir entry (malloc here so we can keep a copy of the entry around and
        // store it in the global file table)
        directory_entry *ptr_to_dir_entry = (directory_entry *)malloc(sizeof(directory_entry));
        uint16_t dir_entry_block_num;
        uint8_t dir_entry_idx;
        if (ptr_to_dir_entry == NULL)
        {
            return EK_OPEN_MALLOC_FAILED;
        }
        int find_dir_entry_status = find_file_in_root_dir(fname, ptr_to_dir_entry, &dir_entry_block_num, &dir_entry_idx);
        if (find_dir_entry_status < 0)
        { // indicates an error
            return EK_OPEN_FIND_FILE_IN_ROOT_DIR_FAILED;
        }

        // case: we couldn't find the file
        if (find_dir_entry_status != RFIND_FILE_IN_ROOT_DIR_FILE_FOUND)
        {
            // RFIND_FILE_IN_ROOT_DIR_FILE_NOT_FOUND or RFIND_FILE_IN_ROOT_DIR_FILE_DELETED
            if (mode == F_READ)
            {
                return EK_OPEN_FILE_DOES_NOT_EXIST;
            }

            // create a dir entry
            time_t mtime = time(NULL);
            if (mtime == (time_t)-1)
            {
                return EK_OPEN_TIME_FAILED;
            }

            *ptr_to_dir_entry = (directory_entry){
                .name = {0},
                .size = 0,
                .first_block = 0,
                .type = 1,
                .perm = P_READ_WRITE_FILE_PERMISSION, // read and write
                .mtime = mtime,
                .padding = {0}};
            strcpy(ptr_to_dir_entry->name, fname); // can safely use strcpy because we checked fname

            // write the dir entry
            if (write_new_root_dir_entry(ptr_to_dir_entry, &dir_entry_block_num, &dir_entry_idx) != 0)
            {
                return EK_OPEN_WRITE_NEW_ROOT_DIR_ENTRY_FAILED;
            }
        }
        // create an entry in the global file table
        global_fd_table[fd_idx] = (global_fd_entry){
            .ref_count = 1,
            .dir_entry_block_num = dir_entry_block_num,
            .dir_entry_idx = dir_entry_idx,
            .ptr_to_dir_entry = ptr_to_dir_entry,
            .write_locked = mode, // 0 for read, 1 for write, 2 for append
            .offset = 0};
    }

    // case: we need to truncate the file because we are opening it for writing
    if (mode == F_WRITE && global_fd_table[fd_idx].ptr_to_dir_entry->size > 0)
    {
        // truncate the file
        time_t mtime = time(NULL);
        if (mtime == (time_t)-1)
        {
            return EK_OPEN_TIME_FAILED;
        }
        global_fd_table[fd_idx].ptr_to_dir_entry->size = 0;
        global_fd_table[fd_idx].ptr_to_dir_entry->mtime = mtime;
        if (global_fd_table[fd_idx].ptr_to_dir_entry->first_block != 0)
        {
            clear_fat_file(global_fd_table[fd_idx].ptr_to_dir_entry->first_block);
        }
        global_fd_table[fd_idx].ptr_to_dir_entry->first_block = 0;

        // write the dir entry
        if (write_root_dir_entry(global_fd_table[fd_idx].ptr_to_dir_entry, global_fd_table[fd_idx].dir_entry_block_num, global_fd_table[fd_idx].dir_entry_idx) != 0)
        {
            return EK_OPEN_WRITE_ROOT_DIR_ENTRY_FAILED;
        }
    }

    uint32_t offset = 0;
    if (mode == F_APPEND)
    {
        offset = global_fd_table[fd_idx].ptr_to_dir_entry->size;
    }
    else if (mode == F_WRITE)
    {
        offset = 0;
    }
    else if (mode == F_READ)
    {
        offset = global_fd_table[fd_idx].offset;
    }
    global_fd_table[fd_idx].write_locked = mode;
    global_fd_table[fd_idx].offset = offset;

    return (int)fd_idx; // semi-safe cast because uint16_t should fit in int on most systems
}

int k_close(int fd)
{
    if (!is_mounted())
    {
        return EFS_NOT_MOUNTED;
    }

    if (fd >= GLOBAL_FD_TABLE_SIZE || fd < 0)
    {
        return EK_CLOSE_FD_OUT_OF_RANGE;
    }
    if (fd == STDIN_FD || fd == STDOUT_FD || fd == STDERR_FD)
    {
        return EK_CLOSE_SPECIAL_FD;
    }

    if (global_fd_table[fd].ref_count > 0) // just in case, we don't want to decrement a ref count that is already 0 (though this should never happen)
    {
        global_fd_table[fd].ref_count -= 1;
    }
    if (global_fd_table[fd].ref_count == 0)
    {
        // if the file was marked as deleted but still referenced
        // then walk the FAT and zero it out
        if (global_fd_table[fd].ptr_to_dir_entry->name[0] == 2)
        {
            uint16_t first_block = global_fd_table[fd].ptr_to_dir_entry->first_block;
            if (first_block != 0)
            {
                clear_fat_file(first_block);
            }

            // mark the file as deleted and write it through
            directory_entry *ptr_to_dir_entry = global_fd_table[fd].ptr_to_dir_entry;
            uint16_t dir_entry_block_num = global_fd_table[fd].dir_entry_block_num;
            uint8_t dir_entry_idx = global_fd_table[fd].dir_entry_idx;
            ptr_to_dir_entry->name[0] = 1;
            ptr_to_dir_entry->first_block = 0;
            if (write_root_dir_entry(ptr_to_dir_entry, dir_entry_block_num, dir_entry_idx) != 0)
            {
                return EK_CLOSE_WRITE_ROOT_DIR_ENTRY_FAILED;
            }
        }

        // free the memory we allocated for the the copy of the directory_entry
        free(global_fd_table[fd].ptr_to_dir_entry);
        global_fd_table[fd].ptr_to_dir_entry = NULL;
        global_fd_table[fd].write_locked = 0;
    }
    return 0;
}

int k_read(int fd, int n, char *buf)
{
    if (!is_mounted())
    {
        return EFS_NOT_MOUNTED;
    }

    if (fd >= GLOBAL_FD_TABLE_SIZE || fd < 0)
    {
        return EK_READ_FD_OUT_OF_RANGE;
    }

    if (fd == STDIN_FD || fd == STDOUT_FD || fd == STDERR_FD)
    {
        // special case read for stdin, stdout, stderr
        // we just read from the buffer
        int bytes_read = read(fd, buf, n); // NOTE: unix systems typically assign STDIN, STDOUT, STDERR in the same way we do, so this should work
        // We allow reading from stdin here because it's possible that there has been some redirection
        // so we let it play out
        if (bytes_read < 0)
        {
            return EK_READ_READ_FAILED;
        };
        return bytes_read;
    }

    global_fd_entry *fd_entry = &global_fd_table[fd];
    if (fd_entry->ref_count == 0)
    {
        return EK_READ_FD_NOT_IN_TABLE;
    }

    uint32_t file_size = fd_entry->ptr_to_dir_entry->size;
    uint32_t offset = fd_entry->offset;
    uint16_t block_size = fs.block_size;
    uint8_t perm = fd_entry->ptr_to_dir_entry->perm;

    // can't read
    if (perm < P_READ_ONLY_FILE_PERMISSION)
    {
        return EK_READ_WRONG_PERMISSIONS;
    }

    // if the offset is at or past the end of the file, there's nothing for us to read
    // this also checks that the file is non-empty
    if (offset >= file_size)
    {
        return 0;
    }

    // if n = 0, then we just return 0
    if (n == 0)
    {
        return 0;
    }

    // first we need to get to the offset, which requires traversing some number of blocks
    uint16_t block = fd_entry->ptr_to_dir_entry->first_block; // expect that this is non-zero since the file is non empty
    uint16_t n_blocks_to_skip = offset / block_size;          // TODO: is there any chance that offset means the n_blocks_to_skip exceeds uint16_t size?
    uint16_t offset_in_block = offset % block_size;

    // skip to the right block
    for (int i = 0; i < n_blocks_to_skip; i++)
    {
        if (next_block_num(block, &block) != 0)
        {
            // TODO: we should separate out the error case when n_blocks_to_skip is simply too many jumps
            // and we end up jumping to to FFFF!
            return EK_READ_COULD_NOT_JUMP_TO_BLOCK_FOR_OFFSET;
        }
    }

    // start reading the blocks sequentialy and then memcpy-ing them out
    char *char_buf = (char *)fs.block_buf;
    int n_copied = 0;
    n = min(n, file_size - offset); // read at most the rest of the file
    while (n > n_copied && block != FAT_END_OF_FILE)
    { // NOTE: we shouldn't need to check for EOF here since we won't read more than the file size, but just in case
        get_block(block, fs.block_buf);
        // we want to read at most the rest of the block
        // but if n is smaller than that, then we should only read n
        uint16_t n_to_copy = min(n - n_copied, block_size - offset_in_block);
        memcpy(buf + n_copied, char_buf + offset_in_block, n_to_copy);
        n_copied += n_to_copy;
        // read the next block from the start
        offset_in_block = 0;
        // identify the next block
        next_block_num(block, &block);
    }
    // increment the file offset by the number of bytes read
    fd_entry->offset += n_copied;
    return n_copied;
}

int64_t k_lseek(int fd, int offset, int whence)
{
    if (!is_mounted())
    {
        return EFS_NOT_MOUNTED;
    }

    if (fd >= GLOBAL_FD_TABLE_SIZE || fd < 0)
    {
        return EK_LSEEK_FD_OUT_OF_RANGE;
    }

    if (fd == STDIN_FD || fd == STDOUT_FD || fd == STDERR_FD)
    {
        return EK_LSEEK_SPECIAL_FD;
    }

    global_fd_entry *fd_entry = &global_fd_table[fd];
    if (fd_entry->ref_count == 0)
    {
        return EK_LSEEK_FD_NOT_IN_TABLE;
    }

    if (whence != F_SEEK_SET && whence != F_SEEK_CUR && whence != F_SEEK_END)
    {
        return EK_LSEEK_BAD_WHENCE;
    }

    // TODO: is this right?
    // can't read = can't lseek
    uint8_t perm = fd_entry->ptr_to_dir_entry->perm;
    if (perm < P_READ_ONLY_FILE_PERMISSION)
    {
        return EK_LSEEK_WRONG_PERMISSIONS;
    }

    // check if total offset would be negative and if so return an error
    uint32_t curr_offset = fd_entry->offset;
    uint32_t size = fd_entry->ptr_to_dir_entry->size;
    uint32_t new_offset;
    if (whence == F_SEEK_SET)
    {
        if (offset < 0)
            return EK_LSEEK_NEGATIVE_OFFSET;
        // NOTE: no possibility of overflow
        new_offset = offset;
    }
    else if (whence == F_SEEK_CUR)
    {
        if (offset < 0 && -offset > curr_offset)
        {
            return EK_LSEEK_NEGATIVE_OFFSET;
        }
        else if ((UINT32_MAX - curr_offset) < offset)
        {
            return EK_LSEEK_OFFSET_OVERFLOW;
        }
        new_offset = curr_offset + offset;
    }
    else
    { // whence == F_SEEK_END
        if (offset < 0 && -offset > size)
        {
            return EK_LSEEK_NEGATIVE_OFFSET;
        }
        else if ((UINT32_MAX - size) < offset)
        {
            return EK_LSEEK_OFFSET_OVERFLOW;
        }
        new_offset = size + offset;
    }

    fd_entry->offset = new_offset;
    return new_offset;
}

int k_write(int fd, const char *str, int n)
{
    if (!is_mounted())
    {
        return EFS_NOT_MOUNTED;
    }

    if (fd >= GLOBAL_FD_TABLE_SIZE || fd < 0)
    {
        return EK_WRITE_FD_OUT_OF_RANGE;
    }

    if (fd == STDIN_FD || fd == STDOUT_FD || fd == STDERR_FD)
    {
        // special case write for stdin, stdout, stderr
        // we just write to the buffer
        int bytes_written = write(fd, str, n); // NOTE: unix systems typically assign STDIN, STDOUT, STDERR in the same way we do, so this should work
        // We allow writing to stdin here because it's possible that there has been some redirection
        // so we let it play out
        if (bytes_written < 0)
        {
            return EK_WRITE_WRITE_FAILED;
        };
        return bytes_written;
    }

    global_fd_entry *fd_entry = &global_fd_table[fd];
    if (fd_entry->ref_count == 0)
    {
        return EK_WRITE_FD_NOT_IN_TABLE;
    }

    uint8_t perm = fd_entry->ptr_to_dir_entry->perm;
    if (perm != P_WRITE_ONLY_FILE_PERMISSION && perm < P_READ_WRITE_AND_EXECUTABLE_FILE_PERMISSION)
    {
        return EK_WRITE_WRONG_PERMISSIONS;
    }

    // if n = 0, then we just return 0
    if (n == 0)
    {
        return 0;
    }

    // if the file is empty, then we need to allocate a new block
    uint32_t file_size = fd_entry->ptr_to_dir_entry->size;
    uint32_t offset = fd_entry->write_locked == F_APPEND ? file_size : fd_entry->offset;
    uint16_t block_size = fs.block_size;
    uint16_t dir_entry_block_num = fd_entry->dir_entry_block_num;
    uint8_t dir_entry_idx = fd_entry->dir_entry_idx;
    bool is_writing_new_blocks = false; // we'll use this variable to track whether the block we're at is a new one (meaning we need to fetch the block from disk) or an old one (meaning we can just write to it)

    // Get the first block of the file
    if (fd_entry->ptr_to_dir_entry->first_block == 0)
    {
        uint16_t new_block = first_empty_block();
        if (new_block == 0)
        {
            return 0;
        }
        fs.fat[new_block] = FAT_END_OF_FILE;
        fd_entry->ptr_to_dir_entry->first_block = new_block;
    }
    uint16_t block = fd_entry->ptr_to_dir_entry->first_block;

    // Get the idx of the block we're writing to
    uint16_t offset_block_idx = (offset + block_size) / block_size; // TODO: is there any chance that offset means the n_blocks_to_skip exceeds uint16_t size?
    uint16_t offset_in_block = offset % block_size;

    // Try to get to the offset_block_idx by
    // using the blocks in the file.
    char *char_buf = (char *)fs.block_buf;
    uint16_t n_blocks_deep = 0; // how many blocks we've traversed
    while (n_blocks_deep < offset_block_idx)
    {
        uint16_t next_block;
        if (next_block_num(block, &next_block) != 0)
        {
            return EK_WRITE_NEXT_BLOCK_NUM_FAILED;
        }
        n_blocks_deep += 1;

        if (next_block == FAT_END_OF_FILE)
        {
            // need to 0 out whatever remains in this block

            // this is the number of bytes in the file that are in the last block
            uint16_t n_file_bytes_in_block = file_size - (n_blocks_deep - 1) * block_size;
            if (get_block(block, char_buf) != 0)
            {
                return EK_WRITE_GET_BLOCK_FAILED;
            }
            memset(char_buf + n_file_bytes_in_block, 0, block_size - n_file_bytes_in_block);
            if (write_block(block, fs.block_buf) != 0)
            {
                return EK_WRITE_WRITE_BLOCK_FAILED;
            }
            break;
        }
        block = next_block;
    }

    // If in the previous step we exhausted the blocks in the file,
    // we continue iterating, up to the offset block, writing each
    // intermediate block as empty
    for (; n_blocks_deep < offset_block_idx; n_blocks_deep++)
    {
        uint16_t prev_block = block;
        block = first_empty_block();
        if (block == 0)
        {
            // no free blocks
            fs.fat[prev_block] = FAT_END_OF_FILE;
            return 0; // we've written 0 bytes since we never got to the offset
        }

        // set FAT linkages
        fs.fat[prev_block] = block;

        // write block as empty
        memset(fs.block_buf, 0, block_size);
        if (write_block(block, fs.block_buf) != 0)
        {
            return EK_WRITE_WRITE_BLOCK_FAILED;
        }
    }

    // At this point, block should point to `offset_block_idx`th block.
    // Recall the examples above:
    // 1. `offset_block_idx` was 2, so block will hold the block number of the
    // newly created block where we'll start writing our file
    // 2. `offset_block_idx` was 2, so block will just hold the block number of the
    // second/last block in the file
    // 3. `offset_block_idx` was 1, block will hold the block number of the first/
    // last block in the file

    int n_copied = 0;
    while (n > n_copied) // NOTE: this check is basically only used to check if n == 0 (following checks occur at the if statement in the loop body)
    {
        // check if we're writing to a new block
        // by comparing the initial size of the block with the current offset
        uint16_t curr_block_idx = (offset + n_copied) / block_size;
        uint16_t origin_file_max_block_idx = (file_size + block_size - 1) / block_size; // file_size - 1 because file_size is effectively 1-indexed
        is_writing_new_blocks = curr_block_idx > origin_file_max_block_idx;
        if (is_writing_new_blocks)
        {
            memset(char_buf, 0, block_size);
        }
        else
        {
            if (get_block(block, char_buf) != 0)
            {
                return EK_WRITE_GET_BLOCK_FAILED;
            }
        }

        uint16_t n_to_copy = min(n - n_copied, block_size - offset_in_block);
        memcpy(char_buf + offset_in_block, str + n_copied, n_to_copy);
        n_copied += n_to_copy; // n_copied out of buf into the file
        if (write_block(block, char_buf))
        {
            return EK_WRITE_WRITE_BLOCK_FAILED;
        }

        if (n_copied >= n) // we're done
        {
            break;
        }

        // read the next block from the start
        offset_in_block = 0;

        // try to get the next block of the file
        uint16_t next_block;
        if (curr_block_idx + 1 < origin_file_max_block_idx)
        {
            if (next_block_num(block, &next_block) != 0)
            {
                return EK_WRITE_NEXT_BLOCK_NUM_FAILED;
            }
        }
        else
        {
            fs.fat[block] = FAT_END_OF_FILE; // prevent a loop where we keep getting the same blocks
            next_block = first_empty_block();
            if (next_block == 0)
            {
                break;
            }
        }
        fs.fat[block] = next_block;
        block = next_block;
    }
    fs.fat[block] = FAT_END_OF_FILE;

    // increment the file offset by the number of bytes written
    fd_entry->offset = offset + n_copied;

    // write the updated at time through to the directory entry
    time_t mtime = time(NULL);
    if (mtime == (time_t)-1)
    {
        return EK_WRITE_TIME_FAILED;
    }
    fd_entry->ptr_to_dir_entry->mtime = mtime;
    fd_entry->ptr_to_dir_entry->size = offset + n_copied;
    // write through to the updated entry to the filesystem
    if (write_root_dir_entry(fd_entry->ptr_to_dir_entry, dir_entry_block_num, dir_entry_idx) != 0)
    {
        // TODO: should we rollback the ptr_to_dir_entry state on failure?
        return EK_WRITE_WRITE_ROOT_DIR_ENTRY_FAILED;
    }
    return n_copied;
}

int k_unlink(const char *fname)
{
    if (!is_mounted())
    {
        return EFS_NOT_MOUNTED;
    }

    // we check for invalid filenames to prevent access to deleted files
    // (e.g., adversarially setting the first byte to 1 or 2 to discover deleted files)
    if (!is_valid_filename(fname))
    {
        return EK_UNLINK_INVALID_FILENAME;
    }

    uint16_t dir_entry_block_num;
    uint8_t dir_entry_idx;
    directory_entry dir_entry; // we may or may not use this; see below
    directory_entry *ptr_to_updated_dir_entry;
    uint16_t fd_idx;

    if (find_file_in_global_fd_table(fname, &fd_idx) == 0)
    {
        // update the directory entry in the fd and set update_dir_entry so it's written through
        global_fd_entry *fd_entry = &global_fd_table[fd_idx];
        ptr_to_updated_dir_entry = fd_entry->ptr_to_dir_entry;
        if (fd_entry->ref_count > 0)
        {
            ptr_to_updated_dir_entry->name[0] = 2; // indicates that it is deleted but still open // TODO: no magic nums
        }
        else
        {
            ptr_to_updated_dir_entry->name[0] = 1;
            if (ptr_to_updated_dir_entry->first_block != 0)
            {
                clear_fat_file(ptr_to_updated_dir_entry->first_block);
            }
        }
        dir_entry_block_num = fd_entry->dir_entry_block_num;
        dir_entry_idx = fd_entry->dir_entry_idx;
    }
    else
    {
        // if we've reached this point, either:
        // - the reference count of the global_fd_entry is 0 or the fname is not in the fd table
        // - the file is already deleted, and thus we can't find it
        // We can't really distinguish between these cases, so we have to look for the file in the
        // root directory
        ptr_to_updated_dir_entry = &dir_entry;
        int find_file_in_root_dir_status = find_file_in_root_dir(fname, ptr_to_updated_dir_entry, &dir_entry_block_num, &dir_entry_idx);
        if (find_file_in_root_dir_status < 0)
        {
            return EK_UNLINK_FIND_FILE_IN_ROOT_DIR_FAILED;
        }
        if (
            find_file_in_root_dir_status == RFIND_FILE_IN_ROOT_DIR_FILE_NOT_FOUND ||
            find_file_in_root_dir_status == RFIND_FILE_IN_ROOT_DIR_FILE_DELETED)
        {
            return EK_UNLINK_FILE_NOT_FOUND;
        }
        ptr_to_updated_dir_entry->name[0] = 1; // mark as deleted
        if (ptr_to_updated_dir_entry->first_block != 0)
        {
            clear_fat_file(ptr_to_updated_dir_entry->first_block);
        }
    }

    // write through to the updated entry to the filesystem
    if (write_root_dir_entry(ptr_to_updated_dir_entry, dir_entry_block_num, dir_entry_idx) != 0)
    {
        return EK_UNLINK_WRITE_ROOT_DIR_ENTRY_FAILED;
    }

    return 0;
}

#define EK_LS_WRITE_FAILED -1
#define EK_LS_FIND_FILE_IN_ROOT_DIR_FAILED -2
#define EK_LS_NOT_IMPLEMENTED -3
int ls_dir_entry(directory_entry *ptr_to_dir_entry)
{
    // TODO: implement nice formatting

    // can write no more than block_size bytes
    // this should be enough since
    // - the block number is at most 65535 (5 bytes to represent)
    // - the permission is 3 bytes
    // - the size is at most 4,294,967,295 which is 10 bytes
    // - the time is at most 18 bytes
    // - the name is at most 31 bytes (since it's a null-terminated string)
    // Including the 4 spaces between the columns, this is 68 bytes. Including the final newline and the null terminator, this is 70 bytes.
    // so our block size of at least 256 is sufficient.

    char *buf = (char *)fs.block_buf;
    int total_bytes_written = 0;
    int n_bytes_written;
    if (ptr_to_dir_entry->first_block != 0)
    {
        n_bytes_written = sprintf(buf, "%3d", ptr_to_dir_entry->first_block);
        if (n_bytes_written < 0)
        {
            return EK_LS_WRITE_FAILED;
        }
        total_bytes_written += n_bytes_written;
    }
    else
    {
        n_bytes_written = sprintf(buf, "    ");
        if (n_bytes_written < 0)
        {
            return EK_LS_WRITE_FAILED;
        }
        total_bytes_written += n_bytes_written;
    }

    // construct the permission string
    char perm_str[5];
    perm_str[0] = (ptr_to_dir_entry->type == 2) ? 'd' : '-';
    perm_str[1] = (ptr_to_dir_entry->perm & 0b100) ? 'r' : '-';
    perm_str[2] = (ptr_to_dir_entry->perm & 0b010) ? 'w' : '-';
    perm_str[3] = (ptr_to_dir_entry->perm & 0b001) ? 'x' : '-';
    perm_str[4] = '\0';

    n_bytes_written = sprintf(buf + n_bytes_written, " %s", perm_str);
    if (n_bytes_written < 0)
    {
        return EK_LS_WRITE_FAILED;
    }
    total_bytes_written += n_bytes_written;

    n_bytes_written = sprintf(buf + total_bytes_written, " %d", ptr_to_dir_entry->size);
    if (n_bytes_written < 0)
    {
        return EK_LS_WRITE_FAILED;
    }
    total_bytes_written += n_bytes_written;

    n_bytes_written = strftime(buf + total_bytes_written, 19, " %b %d %H:%M %Y", localtime(&ptr_to_dir_entry->mtime));
    if (n_bytes_written < 0)
    {
        return EK_LS_WRITE_FAILED;
    }
    total_bytes_written += n_bytes_written;

    n_bytes_written = sprintf(buf + total_bytes_written, " %s\n", ptr_to_dir_entry->name);

    if (n_bytes_written < 0)
    {
        return EK_LS_WRITE_FAILED;
    }
    total_bytes_written += n_bytes_written;

    k_write(STDOUT_FD, buf, total_bytes_written);

    return 0;
}

int k_ls(const char *filename)
{
    if (filename != NULL)
    {
        directory_entry dir_entry;
        uint16_t dir_entry_block_num;
        uint8_t dir_entry_idx;
        if (find_file_in_root_dir(filename, &dir_entry, &dir_entry_block_num, &dir_entry_idx) != 0)
        {
            // either failed or the file doesn't exist (we don't distinguish here)
            return EK_LS_FIND_FILE_IN_ROOT_DIR_FAILED;
        }
        return ls_dir_entry(&dir_entry);
    }

    uint16_t block = 1;
    // we'll need a second buffer here, since ls_dir_entry uses the fs.block_buf
    // and we'll need to keep track of the block as we go
    directory_entry *dir_entry_buf = (directory_entry *)malloc(fs.block_size);
    if (dir_entry_buf == NULL)
    {
        return EK_LS_MALLOC_FAILED;
    }

    // n_dir_entry_per_block is at most 4096 / 64 = 64
    uint8_t n_dir_entry_per_block = fs.block_size / sizeof(directory_entry);
    int status;
    while (true)
    {
        if (get_block(block, dir_entry_buf) != 0)
        {
            status = EK_LS_GET_BLOCK_FAILED;
            goto cleanup;
        }

        for (uint8_t i = 0; i < n_dir_entry_per_block; i++)
        {
            directory_entry curr_dir_entry = dir_entry_buf[i];
            if (curr_dir_entry.name[0] == 0)
            {
                status = 0;
                goto cleanup;
            }
            if (curr_dir_entry.name[0] == 1 || curr_dir_entry.name[0] == 2)
            {
                continue;
            }

            ls_dir_entry(&curr_dir_entry);
        }

        if (next_block_num(block, &block) != 0)
        {
            status = EK_LS_NEXT_BLOCK_NUM_FAILED;
            goto cleanup;
        }
    };

cleanup:
    free(dir_entry_buf);
    return status;
}

int k_chmod(const char *fname, uint8_t perm)
{
    if (!is_mounted())
    {
        return EFS_NOT_MOUNTED;
    }

    // we check for invalid filenames to prevent access to deleted files
    // (e.g., adversarially setting the first byte to 1 or 2 to discover deleted files)
    if (!is_valid_filename(fname))
    {
        return EK_CHMOD_INVALID_FILENAME;
    }

    uint16_t dir_entry_block_num;
    uint8_t dir_entry_idx;
    directory_entry dir_entry;
    if (find_file_in_root_dir(fname, &dir_entry, &dir_entry_block_num, &dir_entry_idx) != 0)
    {
        return EK_CHMOD_FILE_NOT_FOUND;
    }

    if (perm < 0 || perm > 7)
    {
        return EK_CHMOD_WRONG_PERMISSIONS;
    }

    dir_entry.perm = perm;
    if (write_root_dir_entry(&dir_entry, dir_entry_block_num, dir_entry_idx) != 0)
    {
        return EK_CHMOD_WRITE_ROOT_DIR_ENTRY_FAILED;
    }

    return 0;
}

int k_mv(const char *src, const char *dest)
{
    if (!is_mounted())
    {
        return EFS_NOT_MOUNTED;
    }

    if (!is_valid_filename(dest))
    {
        return EK_MV_INVALID_FILENAME;
    }

    // if the dest file exists, we need to unlink it
    int unlink_status = k_unlink(dest);
    if (unlink_status != 0 && unlink_status != EK_UNLINK_FILE_NOT_FOUND) // it's OK if the dest file doesn't exist
    {
        return EK_MV_UNLINK_FAILED;
    }

    int src_fd = k_open(src, F_READ); // we only need to check read permissions here
                                      // since the actual contents will not change and
                                      // so we don't need to make sure there are no simultaneous
                                      // writers
    if (src_fd == EK_OPEN_FILE_DOES_NOT_EXIST)
    {
        return EK_MV_FILE_NOT_FOUND;
    }
    else if (src_fd < 0)
    {
        return EK_MV_OPEN_FAILED;
    }

    global_fd_entry *src_fd_entry = &global_fd_table[src_fd];
    strcpy(src_fd_entry->ptr_to_dir_entry->name, dest); // know dest must be a valid filename because we opened it

    int status = 0;
    if (write_root_dir_entry(src_fd_entry->ptr_to_dir_entry, src_fd_entry->dir_entry_block_num, src_fd_entry->dir_entry_idx) != 0)
    {
        status = EK_MV_WRITE_ROOT_DIR_ENTRY_FAILED;
        goto cleanup;
    }

cleanup:
    if (k_close(src_fd) != 0)
    {
        status = EK_MV_CLOSE_FAILED;
    }
    return status;
}
