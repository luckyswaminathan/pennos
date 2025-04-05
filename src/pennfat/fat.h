#include <stdint.h>
#include <stddef.h>

#define EMOUNT_BAD_FAT_FIRST_ENTRY 1
#define EMOUNT_OPEN_FAILED 5
#define EMOUNT_MMAP_FAILED 6
#define EMOUNT_CLOSE_FAILED 7
#define EMOUNT_READ_FAILED 8
#define EMOUNT_CLOSE_FAILED_MUNMAP_FAILED 9

#define EUNMOUNT_MUNMAP_FAILED 1

typedef struct fat16_fs_st {
    uint16_t* fat;
    size_t fat_size; // the total sizei of the fat
    uint16_t block_size;
    uint16_t blocks_in_fat; 
} fat16_fs;

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
