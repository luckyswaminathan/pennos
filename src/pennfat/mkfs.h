#ifndef PENNFAT_MKFS_H
#define PENNFAT_MKFS_H

#define BAD_BLOCKS_IN_FAT_VAL 1
#define BAD_BLOCK_SIZE_CONFIG_VAL 2
#define BAD_FS_NAME_VAL 3
#define MKFS_UNKNOWN_ERROR 4
#define EMKFS_OPEN_FAILED 5
#define EMKFS_WRITE_FAILED 6
#define EMKFS_WRITE_LESS 7 // wrote less than expected to
#define EMKFS_LSEEK_FAILED 8
#define EMKFS_CALLOC_FAILED 9
#define EMKFS_CLOSE_FAILED 10

#include <stdint.h>

/**
 * Create a new filesystem
 *
 * Returns 0 on success and a non-zero error code on error
 */
int mkfs(char* fs_name, uint8_t blocks_in_fat, uint8_t block_size_config);

#endif // PENNFAT_MKFS_H
