#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <time.h>

#define EMOUNT_BAD_FAT_FIRST_ENTRY 1
#define EMOUNT_MALLOC_FAILED 4
#define EMOUNT_OPEN_FAILED 5
#define EMOUNT_MMAP_FAILED 6
#define EMOUNT_READ_FAILED 8

#define EUNMOUNT_MUNMAP_FAILED 1
#define EUNMOUNT_CLOSE_FAILED 1

#define F_WRITE 0
#define F_READ 1
#define F_APPEND 2

typedef struct fat16_fs_st {
    uint16_t* fat;
    size_t fat_size; // the total size of the fat
    uint16_t block_size;
    uint16_t blocks_in_fat; 
    int fd; // fd to the file of the FAT
    void* block_buf; // buffer of size block_size bytes
} fat16_fs;

typedef struct directory_entry_st {
    char name[32];       // 32 bytes
    uint32_t size;       // 4
    uint16_t first_block; // 2
    uint8_t type;        // 1
    uint8_t perm;        // 1
    time_t mtime;        // 8
    char padding[16];    // 16
} directory_entry; // 64 bytes in total!
_Static_assert(sizeof(directory_entry) == 64, "directory_entry must be 64 byes");

typedef struct global_fd_entry_st {
    size_t ref_count;
    directory_entry* ptr_to_dir_entry; // an in-memory copy of the dir entry. This should be maintained so it always matches what is on disk
    uint16_t dir_entry_block_num;
    uint8_t dir_entry_idx; 
    bool write_locked; // mutex for whether this file is already being written to by another file
    uint32_t offset; // offset can be no greater than the size, which is specified in the directory entry
} global_fd_entry;

/**
 * Mount the pennfat (fat16) filesystem from the file named fs_name into
 * the struct pointed to by ptr_to_fs.
 *
 * Return 0 on success, and an error code on error (see the EMOUNT_* error codes
 * defined in this header).
 */
int mount(char *fs_name, fat16_fs* ptr_to_fs);

/**
 * Unmount the pennfat (fat16) filesystem from the struct pointed to by ptr_to_fs.
 * This function will 0 out the struct on success (but may not on failure).
 *
 * Return 0 on success, and an error code on error (see the EUNMOUNT_* error codes
 * defined in this header).
 */
int unmount(fat16_fs* ptr_to_fs);

/**
 * TODO
 */
int k_open(fat16_fs* ptr_to_fs, const char* fname, int mode);

int k_close(fat16_fs* ptr_to_fs, int fd);

int k_write(fat16_fs* ptr_to_fs, int fd, const char *str, int n);

int k_unlink(fat16_fs* ptr_to_fs, const char* fname);
