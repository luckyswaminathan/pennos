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

global_fd_entry global_fd_table[GLOBAL_FD_TABLE_SIZE] = {0};

int min(int a, int b) {
    if (a < b) return a;
    return b;
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
    return ptr_to_fs->fat_size + ((size_t)block_num-1) * ptr_to_fs->block_size;
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
// TODO: add a return condition for already being at the end file 

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
    *next_block_num = ptr_to_fs->fat[block_num];
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
int find_file_in_root_dir(fat16_fs* ptr_to_fs, const char* fname, directory_entry* ptr_to_dir_entry, uint16_t* ptr_to_block, uint8_t* ptr_to_dir_entry_idx) {
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
                // memcpy into the passed buffer so we're not
                // pointing to the ptr_to_fs->block_buf, which could
                // change
                memcpy(ptr_to_dir_entry, dir_entry_buf + i, sizeof(directory_entry));
                *ptr_to_block = block;
                *ptr_to_dir_entry_idx = i;

                // TODO: the below should never happen
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
 *
 * Note that this function skips files that have 0,1 or 2 as the first byte of their name
 * because these entries are not valid entries (in fact, they've have their name mangled, so
 * they should not match any but an adversarial fname). It also skips files with a ref_count of 0
 */
int find_file_in_global_fd_table(const char* fname, uint16_t* ptr_to_fd_idx) {
    for (uint16_t i = 0; i < GLOBAL_FD_TABLE_SIZE; i++) {
        // use strcmp because both fname and the directory entries in the fat/global fd table should
        // have been checked for proper null termination
        if (global_fd_table[i].ref_count == 0 || global_fd_table[i].ptr_to_dir_entry->name[0] < 3) continue;
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

int find_empty_spot_in_root_dir(fat16_fs* ptr_to_fs, uint16_t* ptr_to_block, uint8_t* ptr_to_offset) {
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

#define EWRITE_ROOT_DIR_ENTRY_GET_BLOCK_FAILED 1
#define EWRITE_ROOT_DIR_ENTRY_NO_EMPTY_BLOCKS 2
#define EWRITE_ROOT_DIR_ENTRY_WRITE_BLOCK_FAILED 3

int write_root_dir_entry(fat16_fs* ptr_to_fs, directory_entry* ptr_to_dir_entry, uint16_t block, uint8_t directory_entry_offset) {
    directory_entry* dir_entry_buf = (directory_entry*) ptr_to_fs->block_buf;
    if (get_block(ptr_to_fs, block, dir_entry_buf) != 0) {
        return EWRITE_ROOT_DIR_ENTRY_GET_BLOCK_FAILED; 
    }

    // the last entry of the end_block can now store our dir_entry (replacing the directory_entry that represents
    // end of directory)
    dir_entry_buf[directory_entry_offset] = *ptr_to_dir_entry;
    
    // write the block
    if (write_block(ptr_to_fs, block, dir_entry_buf) != 0) {
        return EWRITE_ROOT_DIR_ENTRY_WRITE_BLOCK_FAILED;
    }
    return 0;
}

#define EWRITE_NEW_ROOT_DIR_ENTRY_FIND_EMPTY_SPOT_IN_ROOT_DIR_FAILED 1
#define EWRITE_NEW_ROOT_DIR_ENTRY_WRITE_ROOT_DIR_ENTRY_FAILED 2

int write_new_root_dir_entry(fat16_fs* ptr_to_fs, directory_entry* ptr_to_dir_entry, uint16_t* ptr_to_block, uint8_t* ptr_to_dir_entry_idx) {
    // look for an empty spot in the root directory
    uint16_t block; // the block where we found an empty spot
    uint8_t directory_entry_offset; // the offset (in terms of directory_entry entries) where we found the empty spot

    int find_empty_spot_status = find_empty_spot_in_root_dir(ptr_to_fs, &block, &directory_entry_offset);
    if (find_empty_spot_status < 0) {
        return EWRITE_NEW_ROOT_DIR_ENTRY_FIND_EMPTY_SPOT_IN_ROOT_DIR_FAILED;
    }
    
    bool is_last_in_block = directory_entry_offset == (ptr_to_fs->block_size / sizeof(directory_entry) - 1);
    if (find_empty_spot_status == RFIND_EMPTY_SPOT_IN_ROOT_DIR_END_ENTRY && !is_last_in_block) {
        // case where we need to create an entry for the end of the block
        directory_entry empty_dir_entry = {0};
        write_root_dir_entry(ptr_to_fs, &empty_dir_entry, block, directory_entry_offset + 1); 
    } else if (find_empty_spot_status == RFIND_EMPTY_SPOT_IN_ROOT_DIR_END_ENTRY && is_last_in_block) {
        // case where we need to allocate a new block for the directory

        uint16_t empty_block = first_empty_block(ptr_to_fs);
        if (empty_block == 0) {
            return EWRITE_ROOT_DIR_ENTRY_NO_EMPTY_BLOCKS;
        }

        // zero out the new block
        // this makes the first directory_entry in the new block
        // the end directory entry
        memset(ptr_to_fs->block_buf, 0, ptr_to_fs->block_size);
        if (write_block(ptr_to_fs, empty_block, ptr_to_fs->block_buf) != 0) {
            return EWRITE_ROOT_DIR_ENTRY_WRITE_BLOCK_FAILED;
        }
    
        // add the new block to the FAT
        ptr_to_fs->fat[block] = empty_block;
        ptr_to_fs->fat[empty_block] = FAT_END_OF_FILE;
    }

    if (write_root_dir_entry(ptr_to_fs, ptr_to_dir_entry, block, directory_entry_offset) != 0) {
        return EWRITE_NEW_ROOT_DIR_ENTRY_WRITE_ROOT_DIR_ENTRY_FAILED;
    }
    *ptr_to_block = block;
    *ptr_to_dir_entry_idx = directory_entry_offset;
    return 0;
}

uint16_t find_empty_fd() {
    // find an empty fd_idx // TODO: refactor into its own function
    uint16_t fd_idx = GLOBAL_FD_TABLE_ENTRY_NOT_FOUND_SENTINEL;
    for (uint16_t i = 0; i < GLOBAL_FD_TABLE_SIZE; i++) {
        if (global_fd_table[i].ref_count == 0) {
            fd_idx = i;
            break;
        }
    }
    return fd_idx;
}

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
    // NOTE: because of the above validation, you can never open a deleted file
    // or end dir entry since they start with 0, 1, or 2 which are not part of the
    // valid filename charset

    uint16_t fd_idx;
    int find_file_in_global_fd_table_status = find_file_in_global_fd_table(fname, &fd_idx);

    // Already found the file in the global_fd_table
    if (find_file_in_global_fd_table_status == 0) {
        global_fd_entry fd_entry = global_fd_table[fd_idx];

        // we already have an entry in the global file table
        if (fd_entry.write_locked && acquire_write_lock) {
            return EK_OPEN_ALREADY_WRITE_LOCKED;
        }
    
        uint8_t perm = fd_entry.ptr_to_dir_entry->perm;
        if (
                perm == P_NO_FILE_PERMISSION ||
                perm == P_WRITE_ONLY_FILE_PERMISSION && mode == F_READ || // NOTE: F_WRITE also allows reading, but we only gate this on the f_write function
                (perm == P_READ_ONLY_FILE_PERMISSION || perm == P_READ_AND_EXECUTABLE_FILE_PERMISSION) && (mode == F_WRITE || mode == F_APPEND)
        ) {
            return EK_OPEN_WRONG_PERMISSIONS;
        }

        // acquire the write lock
        global_fd_table[fd_idx].write_locked = acquire_write_lock;
        return (int) fd_idx; // semi-safe cast because uint16_t should fit in int on most systems
    }
    
    // Otherwise: the fd_idx was not found so we need to make a new entry
    // in the global file table
    fd_idx = find_empty_fd();
    if (fd_idx == GLOBAL_FD_TABLE_ENTRY_NOT_FOUND_SENTINEL) {
        return EK_OPEN_GLOBAL_FD_TABLE_FULL;
    }

    // find or create a dir entry (malloc here so we can keep a copy of the entry around and
    // store it in the global file table)
    directory_entry* ptr_to_dir_entry = (directory_entry*) malloc(sizeof(directory_entry));
    uint16_t dir_entry_block_num;
    uint8_t dir_entry_idx;
    if (ptr_to_dir_entry == NULL) {
        return EK_OPEN_MALLOC_FAILED;
    }
    int find_dir_entry_status = find_file_in_root_dir(ptr_to_fs, fname, ptr_to_dir_entry, &dir_entry_block_num, &dir_entry_idx);
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
            .perm = P_READ_WRITE_FILE_PERMISSION, // read and write
            .mtime = mtime,
            .padding = {0}
        };
        strcpy(ptr_to_dir_entry->name, fname); // can safely use strcpy because we checked fname

        // write the dir entry
        if (write_new_root_dir_entry(ptr_to_fs, ptr_to_dir_entry, &dir_entry_block_num, &dir_entry_idx) != 0) {
            return EK_OPEN_WRITE_NEW_ROOT_DIR_ENTRY_FAILED; 
        }
    }

    // create an entry in the global file table
    global_fd_table[fd_idx] = (global_fd_entry) {
        .ref_count = 1,
        .dir_entry_block_num = dir_entry_block_num,
        .dir_entry_idx = dir_entry_idx,
        .ptr_to_dir_entry = ptr_to_dir_entry,
        .write_locked = acquire_write_lock
    };
    return (int) fd_idx; // semi-safe cast because uint16_t should fit in int on most systems
}

/**
 * Clear a file starting at block. It is expected that block is 
 * the first block in the file. If it is not
 *
 * Note that this function does not validate that the blocks at each
 * stage (including the initially passed block) are in bounds. It assumes
 * the FAT is maintained as valid.
 */
void clear_fat_file(fat16_fs* ptr_to_fs, uint16_t block) {
    while (block != FAT_END_OF_FILE) {
        uint16_t next_block = ptr_to_fs->fat[block];
        ptr_to_fs->fat[block] = 0;
        block = next_block;
    }
}

int k_close(fat16_fs* ptr_to_fs, int fd) {
    if (fd >= GLOBAL_FD_TABLE_SIZE || fd < 0) {
        return EK_CLOSE_FD_OUT_OF_RANGE;
    }
    
    global_fd_table[fd].ref_count -= 1;
    if (global_fd_table[fd].ref_count == 0) {
        // if the file was marked as deleted but still referenced
        // then walk the FAT and zero it out
        if (global_fd_table[fd].ptr_to_dir_entry->name[0] == 2) {
            uint16_t first_block = global_fd_table[fd].ptr_to_dir_entry->first_block;
            clear_fat_file(ptr_to_fs, first_block);

            // mark the file as deleted and write it through 
            directory_entry* ptr_to_dir_entry = global_fd_table[fd].ptr_to_dir_entry; 
            uint16_t dir_entry_block_num = global_fd_table[fd].dir_entry_block_num;
            uint8_t dir_entry_idx = global_fd_table[fd].dir_entry_idx; 
            ptr_to_dir_entry->name[0] = 1;
            if (write_root_dir_entry(ptr_to_fs, ptr_to_dir_entry, dir_entry_block_num, dir_entry_idx) != 0) {
                return EK_CLOSE_WRITE_ROOT_DIR_ENTRY_FAILED; 
            }
        }

        // free the memory we allocated for the the copy of the directory_entry
        free(global_fd_table[fd].ptr_to_dir_entry);
        global_fd_table[fd].ptr_to_dir_entry = NULL;
        global_fd_table[fd].write_locked = false;
    }
    return 0;
}


int k_read(fat16_fs* ptr_to_fs, int fd, int n, char* buf) {
    if (fd >= GLOBAL_FD_TABLE_SIZE || fd < 0) {
        return EK_READ_FD_OUT_OF_RANGE;
    }

    global_fd_entry fd_entry = global_fd_table[fd];
    if (fd_entry.ref_count == 0) {
        return EK_READ_FD_NOT_IN_TABLE;
    }

    // expect that the fd_entry is fully valid (e.g., first_block should be non-zero etc)
    uint32_t file_size = fd_entry.ptr_to_dir_entry->size;
    uint32_t offset = fd_entry.offset;
    uint16_t block_size = ptr_to_fs->block_size;
    uint16_t block = fd_entry.ptr_to_dir_entry->first_block;
    uint8_t perm = fd_entry.ptr_to_dir_entry->perm;

    // can't read
    if (perm < P_READ_ONLY_FILE_PERMISSION) {
        return EK_READ_WRONG_PERMISSIONS;
    }

    // if the offset is at or past the end of the file, there's nothing for us to read
    if (offset >= file_size) {
        return 0;
    }

    // first we need to get to the offset, which requires traversing some number of blocks
    uint16_t n_blocks_to_skip = fd_entry.offset / block_size; // TODO: is there any chance that offset means the n_blocks_to_skip exceeds uint16_t size?
    uint16_t offset_in_block = fd_entry.offset % block_size;

    // skip to the right block
    for (int i = 0; i < n_blocks_to_skip; i++) {
        if (next_block_num(ptr_to_fs, block, &block) != 0) {
            // TODO: we should separate out the error case when n_blocks_to_skip is simply too many jumps
            // and we end up jumping to to FFFF!
            return EK_READ_COULD_NOT_JUMP_TO_BLOCK_FOR_OFFSET;
        }
    }

    // start reading the blocks sequentialy and then memcpy-ing them out
    char* char_buf = (char*) ptr_to_fs->block_buf;
    int n_copied = 0;
    while (n > n_copied && block != FAT_END_OF_FILE) {
        // TODO: error handling
        get_block(ptr_to_fs, block, ptr_to_fs->block_buf);
        // we want to read at most the rest of the block
        // but if n is smaller than that, then we should only read n
        uint16_t n_to_copy = min(n - n_copied, block_size - offset_in_block);
        memcpy(buf + n_copied, char_buf + offset_in_block, n_to_copy);
        n_copied += n_to_copy;
        // read the next block from the start
        offset_in_block = 0;
        // identify the next block
        next_block_num(ptr_to_fs, block, &block); 
    }
    // increment the file offset by the number of bytes read
    fd_entry.offset += n_copied;
    return n_copied;
}

int64_t k_lseek(int fd, int offset, int whence) {
    if (fd >= GLOBAL_FD_TABLE_SIZE || fd < 0) {
        return EK_LSEEK_FD_OUT_OF_RANGE;
    }

    global_fd_entry fd_entry = global_fd_table[fd];
    if (fd_entry.ref_count == 0) {
        return EK_LSEEK_FD_NOT_IN_TABLE;
    }

    if (whence != F_SEEK_SET && whence != F_SEEK_CUR && whence != F_SEEK_END) {
        return EK_LSEEK_BAD_WHENCE;
    }
    
    // TODO: is this right?
    // can't read = can't lseek
    uint8_t perm = fd_entry.ptr_to_dir_entry->perm;
    if (perm < P_READ_ONLY_FILE_PERMISSION) {
        return EK_LSEEK_WRONG_PERMISSIONS;
    }

    // check if total offset would be negative and if so return an error
    uint32_t curr_offset = fd_entry.offset;
    uint32_t size = fd_entry.ptr_to_dir_entry->size;
    uint32_t new_offset;
    if (whence == F_SEEK_SET) {
        if (offset < 0) return EK_LSEEK_NEGATIVE_OFFSET;
        // NOTE: no possibility of overflow
        new_offset = offset;
    } else if (whence == F_SEEK_CUR) {
        if (offset < 0 && -offset > curr_offset) {
            return EK_LSEEK_NEGATIVE_OFFSET;
        } else if ((UINT32_MAX - curr_offset) < offset) {
            return EK_LSEEK_OFFSET_OVERFLOW;
        }
        new_offset = curr_offset + offset;
    } else if (whence == F_SEEK_END) { 
        if (offset < 0 && -offset > size) {
            return EK_LSEEK_NEGATIVE_OFFSET;
        } else if ((UINT32_MAX - size) < offset) {
            return EK_LSEEK_OFFSET_OVERFLOW;
        }
        new_offset = size + offset;
    }

    fd_entry.offset = new_offset;
    return new_offset;
}

int k_write(fat16_fs* ptr_to_fs, int fd, const char *str, int n) {
    if (fd >= GLOBAL_FD_TABLE_SIZE || fd < 0) {
        return EK_WRITE_FD_OUT_OF_RANGE;
    }

    global_fd_entry fd_entry = global_fd_table[fd];
    if (fd_entry.ref_count == 0) {
        return EK_WRITE_FD_NOT_IN_TABLE;
    }
    

    uint8_t perm = fd_entry.ptr_to_dir_entry->perm;
    if (perm != P_WRITE_ONLY_FILE_PERMISSION && perm < P_READ_WRITE_AND_EXECUTABLE_FILE_PERMISSION) {
        return EK_WRITE_WRONG_PERMISSIONS;
    }
    // set the offset to be the end of the file if in append mode
    // TODO

    // expect that the fd_entry is fully valid (e.g., first_block should be non-zero etc)
    uint32_t file_size = fd_entry.ptr_to_dir_entry->size;
    uint32_t offset = fd_entry.offset;
    uint16_t block_size = ptr_to_fs->block_size;
    uint16_t block = fd_entry.ptr_to_dir_entry->first_block;
    uint16_t dir_entry_block_num = fd_entry.dir_entry_block_num;
    uint8_t dir_entry_idx = fd_entry.dir_entry_idx;

    // first we need to get to the offset, which requires traversing some number of blocks
    //
    // Example:
    // Consider a file that fills up 5 blocks exactly. Let our offset point to 10th byte in the 5th and last
    // block in the file.
    // In that case n_blocks_in_file would be 5, and n_blocks_to_offset would be 4.
    // n_blocks_to_skip would therefore be 4. We would skip 4 blocks and arrive at the
    // block to write to.
    //
    // Example:
    // Consider if our file instead fills up 4 blocks and then 1 byte of the 5th block. Let our offset be the 10th byte
    // of the 5th block.
    // In this case, n_blocks_in_file would be 5, and n_blocks_to_offset would again be 4. However, we would also
    // have some additional writing to do where the offset exceeds the size of the file.
    //
    // Example:
    // Consdider if our file fills up exactly 4 blocks. Let our offset be the 10th byte of the 5th block.
    // In this case, n_blocks_in_file would be 4, and n_blocks_in_offset would be 4. We would again have some additional writing 
    // to do where the offset exceeds the size of the file, this time on a completely new block.
    //
    // Example:
    // Finally, consider if our file fills up exactly 3 blocks. Let our offset be the same as previously.
    // In this case we have to write one fully empty block and then 
    
    // offset_block stores the 1-index of the offset block
    // Example 1:
    // 256 byte blocks. File is 256 bytes. Offset is 256 bytes. Then the offset_block is 2
    // and the offset_in_block = 0
    // 
    // Example 2:
    // Offset is 257 bytes. Then the offset_block is 2 and offset_in_block is 1
    //
    // Example 3:
    // Offset is 255 bytes. Then the offset_block is 1 and the offset_in_block is 255
    uint16_t offset_block = (offset+1) / block_size + (offset % block_size != 0); // TODO: is there any chance that offset means the n_blocks_to_skip exceeds uint16_t size?
    uint16_t offset_in_block = offset % block_size;

    // Try to get to the offset_block by
    // using the blocks in the file.
    char* char_buf = (char*) ptr_to_fs->block_buf;
    uint16_t n_blocks_deep = 0;
    while (n_blocks_deep < offset_block) {
        uint16_t next_block;
        if (next_block_num(ptr_to_fs, block, &next_block) != 0) {
            return EK_WRITE_NEXT_BLOCK_NUM_FAILED;
        }
        n_blocks_deep += 1;

        if (next_block == FAT_END_OF_FILE) {
            // need to 0 out whatever remains in this block
            uint16_t n_file_bytes_in_block = file_size % block_size;
            if (get_block(ptr_to_fs, block, char_buf) != 0) {
                return EK_WRITE_GET_BLOCK_FAILED;
            }
            memset(char_buf + n_file_bytes_in_block, 0, block_size - n_file_bytes_in_block);
            if (write_block(ptr_to_fs, block, ptr_to_fs->block_buf) != 0) {
                return EK_WRITE_WRITE_BLOCK_FAILED;
            }
            break;
        }
        block = next_block;
    }

    // If in the previous step we exhausted the blocks in the file,
    // we continue iterating, up to the offset block, writing each
    // intermediate block as empty
    for (; n_blocks_deep < offset_block; n_blocks_deep++) {
        uint16_t prev_block = block;
        block = first_empty_block(ptr_to_fs);
        if (block == 0) {
            // no free blocks
            ptr_to_fs->fat[prev_block] = FAT_END_OF_FILE;
            return EK_WRITE_NO_EMPTY_BLOCKS;
            // TODO: technically, we shouldn't return an error here
        }

        // set FAT linkages
        ptr_to_fs->fat[prev_block] = block; 

        // write block as empty
        memset(ptr_to_fs->block_buf, 0, block_size);
        if (write_block(ptr_to_fs, block, ptr_to_fs->block_buf) != 0) {
            return EK_WRITE_WRITE_BLOCK_FAILED;
        }
    }

    // At this point, block should point to `offset_block`th block.
    // Recall the examples above:
    // 1. `offset_block` was 2, so block will hold the block number of the
    // newly created block where we'll start writing our file
    // 2. `offset_block` was 2, so block will just hold the block number of the
    // second/last block in the file
    // 3. `offset_block` was 1, block will hold the block number of the first/
    // last block in the file

    bool is_writing_new_blocks = offset >= file_size;
    int n_copied = 0;
    while (n > n_copied) {
        if (is_writing_new_blocks) {
            memset(char_buf, 0, block_size);
        } else {
            get_block(ptr_to_fs, block, char_buf);
        }

        uint16_t n_to_copy = min(n - n_copied, block_size - offset_in_block);
        memcpy(char_buf + offset_in_block, str + n_copied, n_to_copy);
        n_copied += n_to_copy; // n_copied out of buf into the file
        if (write_block(ptr_to_fs, block, char_buf)) {
            return EK_WRITE_WRITE_BLOCK_FAILED; 
        }

        // read the next block from the start
        offset_in_block = 0;

        // try to get the next block of the file
        uint16_t next_block; 
        if (!is_writing_new_blocks) {
            if (next_block_num(ptr_to_fs, block, &next_block) != 0) {
                return EK_WRITE_NEXT_BLOCK_NUM_FAILED;
            }
            if (next_block == FAT_END_OF_FILE) {
                is_writing_new_blocks = true;
            }
        }
        if (is_writing_new_blocks) {
            next_block = first_empty_block(ptr_to_fs);
            if (next_block == 0) {
                // TODO: technically should return 0
                return EK_WRITE_NO_EMPTY_BLOCKS;
            }
        } 
        ptr_to_fs->fat[block] = next_block;
    }

    // increment the file offset by the number of bytes read
    fd_entry.offset += n_copied;
    
    // write the updated at time through to the directory entry
    time_t mtime = time(NULL);
    if (mtime == (time_t) -1) {
        return EK_WRITE_TIME_FAILED;
    }
    fd_entry.ptr_to_dir_entry->mtime = mtime;
    fd_entry.ptr_to_dir_entry->size = offset + n_copied;
    // write through to the updated entry to the filesystem
    if (write_root_dir_entry(ptr_to_fs, fd_entry.ptr_to_dir_entry, dir_entry_block_num, dir_entry_idx) != 0) {
        // TODO: should we rollback the ptr_to_dir_entry state on failure?
        return EK_WRITE_WRITE_ROOT_DIR_ENTRY_FAILED; 
    }
    return n_copied;
}

int k_unlink(fat16_fs* ptr_to_fs, const char* fname) {
    // TODO: add validation for fname here, otherwise it's possible
    // we'll expose filenames that were deleted

    uint16_t dir_entry_block_num;
    uint8_t dir_entry_idx;
    directory_entry* ptr_to_updated_dir_entry;
    uint16_t fd_idx;
    
    if (find_file_in_global_fd_table(fname, &fd_idx) == 0) {
        // update the directory entry in the fd and set update_dir_entry so it's written through 
        ptr_to_updated_dir_entry = global_fd_table[fd_idx].ptr_to_dir_entry;
        if (global_fd_table[fd_idx].ref_count > 0) {
            ptr_to_updated_dir_entry->name[0] = 2; // indicates that it is deleted but still open // TODO: no magic nums
        } else {
            ptr_to_updated_dir_entry->name[0] = 1;
            clear_fat_file(ptr_to_fs, ptr_to_updated_dir_entry->first_block);
        }
        dir_entry_block_num = global_fd_table[fd_idx].dir_entry_block_num;
        dir_entry_idx = global_fd_table[fd_idx].dir_entry_idx;
    } else {
        // if we've reached this point, either:
        // - the reference count of the global_fd_entry is 0 or the fname is not in the fd table
        // - the file is already deleted, and thus we can't find it
        // We can't really distinguish between these cases, so we have to look for the file in the
        // root directory
        
        int find_file_in_root_dir_status = find_file_in_root_dir(ptr_to_fs, fname, ptr_to_updated_dir_entry, &dir_entry_block_num, &dir_entry_idx);
        if (find_file_in_root_dir_status < 0) {
            return EK_UNLINK_FIND_FILE_IN_ROOT_DIR_FAILED; 
        }
        if (
            find_file_in_root_dir_status == RFIND_FILE_IN_ROOT_DIR_FILE_NOT_FOUND || 
            find_file_in_root_dir_status == RFIND_FILE_IN_ROOT_DIR_FILE_DELETED
        ) {
            return EK_UNLINK_FILE_NOT_FOUND;
        }
        ptr_to_updated_dir_entry->name[0] = 1; // mark as deleted
        clear_fat_file(ptr_to_fs, ptr_to_updated_dir_entry->first_block);
    }

    // write through to the updated entry to the filesystem
    if (write_root_dir_entry(ptr_to_fs, ptr_to_updated_dir_entry, dir_entry_block_num, dir_entry_idx) != 0) {
        return EK_UNLINK_WRITE_ROOT_DIR_ENTRY_FAILED; 
    }

    return 0;
}
