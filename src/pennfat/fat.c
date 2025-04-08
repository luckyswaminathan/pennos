#include "src/pennfat/fat.h"
#include "src/pennfat/fat_utils.h"

#include <stdint.h>
#include <stdbool.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

// should be a value storable in a uint16_t
// and less than GLOBAL_FD_TABLE_ENTRY_NOT_FOUND_SENTINEL
// (so at most max(uint16_t) - 1)
#define GLOBAL_FD_TABLE_SIZE 4096
#define GLOBAL_FD_TABLE_ENTRY_NOT_FOUND_SENTINEL 0xFFFF
#define MIN_FILENAME_SIZE 1
#define MAX_FILENAME_SIZE 31
#define FAT_END_OF_FILE 0xFFFF
#define READ_WRITE_FILE_PERMISSION 6

global_fd_entry global_fd_table[GLOBAL_FD_TABLE_SIZE] = {0};

// TODO: move this to fat_utils.h/c
/**
 * Finds the first empty block by walking the fat from index 1. Returns the
 * block index if such a block exists and 0 if there is no empty block
 */
uint16_t first_empty_block(fat16_fs* ptr_to_fs) {
    // look for the first empty block in the fat
    uint16_t n_fat_entries = ptr_to_fs->fat_size / 2;
    for (uint16_t i = 1; i < n_fat_entries; i++) {
        if (ptr_to_fs->fat[i] == 0) {
            return i;
        }
    }
    return 0;
}

int mount(char* fs_name, fat16_fs* ptr_to_fs) {
    int fs_fd = open(fs_name, O_RDWR);
    if (fs_fd == -1) {
        return EMOUNT_OPEN_FAILED;
    }

    // compute the fat size
    uint16_t first_entry;
    if (read(fs_fd, &first_entry, 2) == -1) {
        return EMOUNT_READ_FAILED;
    }

    uint8_t blocks_in_fat;
    uint16_t block_size;
    if (parse_first_fat_entry(first_entry, &block_size, &blocks_in_fat) == -1) {
        return EMOUNT_BAD_FAT_FIRST_ENTRY;
    }

    size_t fat_size = (size_t)blocks_in_fat * block_size;
    uint16_t* fat = mmap(NULL, fat_size, PROT_READ | PROT_WRITE, MAP_SHARED, fs_fd, 0);
    if (fat == MAP_FAILED) {
        return EMOUNT_MMAP_FAILED;
    }

    void* block_buf = malloc(block_size);
    if (block_buf == NULL) {
        return EMOUNT_MALLOC_FAILED;
    }

    *ptr_to_fs = (fat16_fs) {
        .fat = fat,
            .fat_size = fat_size,
            .block_size = block_size,
            .blocks_in_fat = blocks_in_fat,
            .fd = fs_fd,
            .block_buf = block_buf
    };
    return 0;
}

int unmount(fat16_fs* ptr_to_fs) {
    free(ptr_to_fs->block_buf);
    if (munmap(ptr_to_fs->fat, ptr_to_fs->fat_size) == -1) {
        return EUNMOUNT_MUNMAP_FAILED;
    }
    if (close(ptr_to_fs->fd) != 0) {
        return EUNMOUNT_CLOSE_FAILED;
    }
    *ptr_to_fs = (fat16_fs){0};
    ptr_to_fs->fat = NULL; // just to be safe, set the ptr to NULL (in case NULL != 0)
    ptr_to_fs->block_buf = NULL;
    ptr_to_fs->fd = -1;
    return 0;
}

/**
 * Whether the provided string fits thei POSIX filename charset
 */
bool check_filename_charset(const char* str, uint8_t strlen) {
    for (uint8_t i = 0; i < strlen; i++) {
        if (
                !(
                    ('a' <= str[i] && str[i] <= 'z') ||
                    ('A' <= str[i] && str[i] <= 'Z') ||
                    ('0' <= str[i] && str[i] <= '9') ||
                    str[i] == '.' ||
                    str[i] == '_' ||
                    str[i] == '-'
                 )
           ) {
            return false;
        }
    }
    return true;
}

uint32_t get_blocks_in_data_region(fat16_fs* ptr_to_fs) {
    return ((ptr_to_fs->fat_size) / 2) - 1;
}

uint32_t get_byte_offset_of_block(fat16_fs* ptr_to_fs, uint16_t block_num) {
    return ptr_to_fs->fat_size + (size_t)block_num * ptr_to_fs->block_size;
}

#define EGET_BLOCK_BLOCK_NUM_0 1
#define EGET_BLOCK_BLOCK_NUM_TOO_HIGH 2
#define EGET_BLOCK_LSEEK_FAILED 4
#define EGET_BLOCK_READ_FAILED 5
#define EGET_BLOCK_TOO_FEW_BYTES_READ 6

/**
 * Get the data inside of a block and store it into the buffer pointed to
 * by data. It is assumed that the memory pointed to by data has sufficient
 * capacity to store a full block (ptr_to_fs->block_size).
 *
 * Returns 0 on success and an error code on error. See the EGET_BLOCK_* error code.
 */
int get_block(fat16_fs* ptr_to_fs, uint16_t block_num, void* data) {
    uint32_t blocks_in_data_region = get_blocks_in_data_region(ptr_to_fs);
    if (block_num < 1) {
        return EGET_BLOCK_BLOCK_NUM_0;
    }

    if (block_num >= blocks_in_data_region) {
        return EGET_BLOCK_BLOCK_NUM_TOO_HIGH;
    }

    if (lseek(ptr_to_fs->fd, get_byte_offset_of_block(ptr_to_fs, block_num), SEEK_SET) < 0) {
        return EGET_BLOCK_LSEEK_FAILED;
    }

    ssize_t bytes_read = read(ptr_to_fs->fd, data, ptr_to_fs->block_size);
    if (bytes_read == -1) {
        return EGET_BLOCK_READ_FAILED;
    }

    if (bytes_read < ptr_to_fs->block_size) {
        return EGET_BLOCK_TOO_FEW_BYTES_READ;
    }

    return 0;
}

#define ENEXT_BLOCK_NUM_BLOCK_NUM_0 1
#define ENEXT_BLOCK_NUM_BLOCK_NUM_TOO_HIGH 2

/**
 * Get the next block number from the FAT table.
 */
int next_block_num(fat16_fs* ptr_to_fs, uint16_t block_num, uint16_t* next_block_num) {
    uint32_t blocks_in_data_region = get_blocks_in_data_region(ptr_to_fs);
    if (block_num < 1) {
        return ENEXT_BLOCK_NUM_BLOCK_NUM_0;
    }

    if (block_num >= blocks_in_data_region) {
        return ENEXT_BLOCK_NUM_BLOCK_NUM_TOO_HIGH;
    }
    *next_block_num = ptr_to_fs->fat[2 * block_num];
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
int write_block(fat16_fs* ptr_to_fs, uint16_t block_num, void* data) {
    uint32_t blocks_in_data_region = get_blocks_in_data_region(ptr_to_fs);
    if (block_num < 1) {
        return EWRITE_BLOCK_BLOCK_NUM_0;
    }

    if (block_num >= blocks_in_data_region) {
        return EWRITE_BLOCK_BLOCK_NUM_TOO_HIGH;
    }

    if (lseek(ptr_to_fs->fd, get_byte_offset_of_block(ptr_to_fs, block_num), SEEK_SET) < 0) {
        return EWRITE_BLOCK_LSEEK_FAILED;
    }

    ssize_t bytes_written = write(ptr_to_fs->fd, data, ptr_to_fs->block_size);
    if (bytes_written == -1) {
        return EWRITE_BLOCK_WRITE_FAILED;
    }

    if (bytes_written < ptr_to_fs->block_size) {
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
 */
int find_file_in_root_dir(fat16_fs* ptr_to_fs, const char* fname, directory_entry* ptr_to_dir_entry) {
    uint16_t block = 1;
    directory_entry* dir_entry_buf = ptr_to_fs->block_buf;
    // n_dir_entry_per_block is at most 4096 / 64 = 64
    uint8_t n_dir_entry_per_block = ptr_to_fs->block_size / sizeof(directory_entry);

    while (true) {
        if (get_block(ptr_to_fs, block, dir_entry_buf) != 0) {
            return EFIND_FILE_IN_ROOT_DIR_GET_BLOCK_FAILED;
        }

        for (uint8_t i = 0; i < n_dir_entry_per_block; i++) {
            directory_entry curr_dir_entry = dir_entry_buf[i];
            if (curr_dir_entry.name[0] == 0)  {
                return RFIND_FILE_IN_ROOT_DIR_FILE_NOT_FOUND;
            }

            // use strcmp because both fname and the directory entries in the fat should
            // have been checked for proper null termination
            if (strcmp(fname, curr_dir_entry.name) == 0) {
                // memcpy into a newly allocated buffer so we're not
                // pointing to the ptr_to_fs->block_buf, which could
                // change
                memcpy(dir_entry_buf + i, ptr_to_dir_entry, sizeof(directory_entry));

                if (curr_dir_entry.name[0] == 1 || curr_dir_entry.name[0] == 2)  {
                    return RFIND_FILE_IN_ROOT_DIR_FILE_DELETED;
                }
                return RFIND_FILE_IN_ROOT_DIR_FILE_FOUND;
            }
        }

        if (next_block_num(ptr_to_fs, block, &block) != 0) {
            return EFIND_FILE_IN_ROOT_DIR_GET_BLOCK_FAILED;
        }
    };
}

#define RFIND_FILE_IN_GLOBAL_FD_TABLE_NOT_FOUND 1

/**
 * Looks for a file in the global file table matching fname, returning 0 and
 * setting the memory address at ptr_to_fd_idx to the index in the file table if found
 * and returning RFIND_FILE_IN_GLOBAL_FD_TABLE_NOT_FOUND (1) if not found.
 */
int find_file_in_global_fd_table(const char* fname, uint16_t* ptr_to_fd_idx) {
    for (uint16_t i = 0; i < GLOBAL_FD_TABLE_SIZE; i++) {
        // use strcmp because both fname and the directory entries in the fat/global fd table should
        // have been checked for proper null termination
        if (strcmp(global_fd_table[i].ptr_to_dir_entry->name, fname) == 0) {
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

int find_empty_spot_in_root_dir(fat16_fs* ptr_to_fs, uint16_t* ptr_to_block, uint8_t* ptr_to_offset, bool* ptr_to_is_last_in_block) {
    uint8_t directory_entry_offset = 0;
    uint16_t block = 1;
    directory_entry* dir_entry_buf = ptr_to_fs->block_buf;
    uint8_t n_dir_entry_per_block = ptr_to_fs->block_size / sizeof(directory_entry);
    while (true) {
        if (get_block(ptr_to_fs, block, dir_entry_buf) != 0) {
            return EFIND_EMPTY_SPOT_IN_ROOT_DIR_GET_BLOCK_FAILED;
        }

        // n_dir_entry_per_block is at most 4096 / 64 = 64
        for (uint8_t i = 0; i < n_dir_entry_per_block; i++) {
            directory_entry curr_dir_entry = dir_entry_buf[i];
            if (curr_dir_entry.name[0] > 2)  { // not a  special entry
                continue;
            }

            *ptr_to_block = block;
            *ptr_to_offset = i;
            *ptr_to_is_last_in_block = i == n_dir_entry_per_block - 1;
            if (curr_dir_entry.name[0] == 0) {
                return RFIND_EMPTY_SPOT_IN_ROOT_DIR_END_ENTRY;
            }
            if (curr_dir_entry.name[0] == 1 || curr_dir_entry.name[0] == 2) {
                return RFIND_EMPTY_SPOT_IN_ROOT_DIR_DELETED;
            }
        }

        if (next_block_num(ptr_to_fs, block, &block) != 0) {
            return EFIND_EMPTY_SPOT_IN_ROOT_DIR_NEXT_BLOCK_FAILED;
        }
    };
}

#define EWRITE_ROOT_DIR_ENTRY_FIND_EMPTY_SPOT_IN_ROOT_DIR_FAILED 1
#define EWRITE_ROOT_DIR_ENTRY_NO_EMPTY_BLOCKS 2
#define EWRITE_ROOT_DIR_ENTRY_WRITE_BLOCK_FAILED 3

int write_root_dir_entry(fat16_fs* ptr_to_fs, directory_entry* ptr_to_dir_entry) {
    // look for an empty spot in the root directory
    uint16_t block; // the block where we found an empty spot
    uint8_t directory_entry_offset; // the offset (in terms of directory_entry entries) where we found the empty spot
    bool is_last_in_block; // whether where we found the empty spot is the last block

    int find_empty_spot_status = find_empty_spot_in_root_dir(ptr_to_fs, &block, &directory_entry_offset, &is_last_in_block);
    if (find_empty_spot_status < 0) {
        return EWRITE_ROOT_DIR_ENTRY_FIND_EMPTY_SPOT_IN_ROOT_DIR_FAILED;
    }

    // the block we're working on is already in ptr_to_fs->block_buf
    // because find_empty_spot_in_root_dir populates it this way
    directory_entry* dir_entry_buf = (directory_entry*) ptr_to_fs->block_buf;

    // at this point, we can safely use block and directory_entry_offset
    // to write our new directory entry to

    // the last entry of the end_block can now store our dir_entry (replacing the directory_entry that represents
    // end of directory)
    dir_entry_buf[directory_entry_offset] = *ptr_to_dir_entry;

    if (find_empty_spot_status == RFIND_EMPTY_SPOT_IN_ROOT_DIR_END_ENTRY && !is_last_in_block) {
        // case where we need to create an entry for the end of the block

        // set the entry after the current last entry to 0
        memset(dir_entry_buf + directory_entry_offset, 0, sizeof(directory_entry));
    }

    // write the block
    if (write_block(ptr_to_fs, block, dir_entry_buf) != 0) {
        return EWRITE_ROOT_DIR_ENTRY_WRITE_BLOCK_FAILED;
    }
    
    if (find_empty_spot_status == RFIND_EMPTY_SPOT_IN_ROOT_DIR_END_ENTRY && is_last_in_block) {
        // case where we need to allocate a new block

        // if none of the blocks have an empty spot then need to find an empty block to use in the fat
        // TODO: would be amazing if we could make this contiguous (need a better strategy than taking the first_empty_block)
        uint16_t empty_block = first_empty_block(ptr_to_fs);
        if (empty_block == 0) {
            return EWRITE_ROOT_DIR_ENTRY_NO_EMPTY_BLOCKS;
        }


        // zero out the new block
        // this makes the first directory_entry in the new block
        // the end directory entry
        memset(ptr_to_fs->block_buf, 0, ptr_to_fs->block_size);
        if (write_block(ptr_to_fs, empty_block, ptr_to_fs->block_buf) != 0) {
            // TODO: try to revert because we could end up in a bad spot after this (with no end dir entry)
            return EWRITE_ROOT_DIR_ENTRY_WRITE_BLOCK_FAILED;
        }
    
        // add the new block to the FAT
        ptr_to_fs->fat[block] = empty_block;
        ptr_to_fs->fat[empty_block] = FAT_END_OF_FILE;
    }
    return 0;
}

#define EK_OPEN_FILENAME_TOO_LONG -1
#define EK_OPEN_INVALID_FILENAME_CHARSET -2
#define EK_OPEN_FIND_FILE_IN_ROOT_DIR_FAILED -3
#define EK_OPEN_GLOBAL_FD_TABLE_FULL -4
#define EK_OPEN_FILE_DOES_NOT_EXIST -5
#define EK_OPEN_MALLOC_FAILED -6
#define EK_OPEN_TIME_FAILED -7
#define EK_OPEN_ALREADY_WRITE_LOCKED -8
#define EK_OPEN_FILENAME_TOO_SHORT -9
#define EK_OPEN_WRITE_ROOT_DIR_ENTRY_FAILED -10
#define EK_OPEN_NO_EMPTY_BLOCKS -11
int k_open(fat16_fs* ptr_to_fs, const char* fname, int mode) {
    bool acquire_write_lock = (mode == F_WRITE) || (mode == F_APPEND);
    uint8_t strlen = strnlen(fname, MAX_FILENAME_SIZE);
    if (strlen < 1) {
        return EK_OPEN_FILENAME_TOO_SHORT;
    }

    if (fname[strlen] != '\0') {
        // the string must be longer than MAX_FILENAME_SIZE
        return EK_OPEN_FILENAME_TOO_LONG;
    }

    if (!check_filename_charset(fname, strlen)) {
        return EK_OPEN_INVALID_FILENAME_CHARSET;
    }

    uint16_t fd_idx;
    int find_file_in_global_fd_table_status = find_file_in_global_fd_table(fname, &fd_idx);

    // Already found the file in the global_fd_table
    if (find_file_in_global_fd_table_status == 0) {
        // we already have an entry in the global file table
        if (global_fd_table[fd_idx].write_locked && acquire_write_lock) {
            return EK_OPEN_ALREADY_WRITE_LOCKED;
        }
        // acquire the write lock
        global_fd_table[fd_idx].write_locked = acquire_write_lock;
        return (int) fd_idx; // semi-safe cast because uint16_t should fit in int on most systems
    }
    
    // Otherwise: the fd_idx was not found so we need to make a new entry
    // in the global file table

    // find an empty fd_idx // TODO: refactor into its own function
    fd_idx = GLOBAL_FD_TABLE_ENTRY_NOT_FOUND_SENTINEL;
    for (uint16_t i = 0; i < GLOBAL_FD_TABLE_SIZE; i++) {
        if (global_fd_table[i].ref_count == 0) {
            fd_idx = i;
            break;
        }
    }
    if (fd_idx == GLOBAL_FD_TABLE_ENTRY_NOT_FOUND_SENTINEL) {
        return EK_OPEN_GLOBAL_FD_TABLE_FULL;
    }

    // find or create a dir entry (malloc here so we can keep a copy of the entry around and
    // store it in the global file table)
    directory_entry* ptr_to_dir_entry = (directory_entry*) malloc(sizeof(directory_entry));
    if (ptr_to_dir_entry == NULL) {
        return EK_OPEN_MALLOC_FAILED;
    }
    int find_dir_entry_status = find_file_in_root_dir(ptr_to_fs, fname, ptr_to_dir_entry);
    if (find_dir_entry_status < 0) { // indicates an error
        return EK_OPEN_FIND_FILE_IN_ROOT_DIR_FAILED;
    }
    
    // case: we couldn't find the file
    if (find_dir_entry_status != RFIND_FILE_IN_ROOT_DIR_FILE_FOUND) {
        // RFIND_FILE_IN_ROOT_DIR_FILE_NOT_FOUND or RFIND_FILE_IN_ROOT_DIR_FILE_DELETED
        if (mode == F_READ) {
            return EK_OPEN_FILE_DOES_NOT_EXIST;
        }

        // create a dir entry
        time_t mtime = time(NULL);
        if (mtime == (time_t) -1) {
            return EK_OPEN_TIME_FAILED;
        }

        uint16_t empty_block = first_empty_block(ptr_to_fs);
        if (empty_block == 0) {
            return EK_OPEN_NO_EMPTY_BLOCKS;
        }
        ptr_to_fs->fat[empty_block] = FAT_END_OF_FILE;
        *ptr_to_dir_entry = (directory_entry) {
            .name = {0},
            .size = 0,
            .first_block = empty_block,
            .perm = READ_WRITE_FILE_PERMISSION, // read and write
            .mtime = mtime,
            .padding = {0}
        };
        strcpy(ptr_to_dir_entry->name, fname); // can safely use strcpy because we checked fname

        // write the dir entry
        if (write_root_dir_entry(ptr_to_fs, ptr_to_dir_entry) != 0) {
            return EK_OPEN_WRITE_ROOT_DIR_ENTRY_FAILED; 
        }
    }

    // create an entry in the global file table
    global_fd_table[fd_idx] = (global_fd_entry) {
        .ref_count = 0,
        .ptr_to_dir_entry = ptr_to_dir_entry,
        .write_locked = acquire_write_lock
    };
    return (int) fd_idx; // semi-safe cast because uint16_t should fit in int on most systems
}

#define EK_CLOSE_FD_OUT_OF_RANGE -1

int k_close(int fd) {
    if (fd >= GLOBAL_FD_TABLE_SIZE || fd < 0) {
        return EK_CLOSE_FD_OUT_OF_RANGE;
    }
    
    global_fd_table[fd].ref_count -= 1;
    if (global_fd_table[fd].ref_count == 0) {
        // free the memory we allocated for the the copy of the directory_entry
        free(global_fd_table[fd].ptr_to_dir_entry);
        global_fd_table[fd].ptr_to_dir_entry = NULL;
        global_fd_table[fd].write_locked = false;
    }
    return 0;
}

