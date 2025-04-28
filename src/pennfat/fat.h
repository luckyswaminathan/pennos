#ifndef PENNFAT_FAT_H
#define PENNFAT_FAT_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <time.h>
#include "src/pennfat/fat_constants.h"

#define EFS_NOT_MOUNTED 99

#define EMOUNT_BAD_FAT_FIRST_ENTRY 1
#define EMOUNT_ALREADY_MOUNTED 2
#define EMOUNT_MALLOC_FAILED 4
#define EMOUNT_OPEN_FAILED 5
#define EMOUNT_MMAP_FAILED 6
#define EMOUNT_READ_FAILED 8

#define EUNMOUNT_MUNMAP_FAILED 1
#define EUNMOUNT_CLOSE_FAILED 2

#define F_SEEK_SET 1
#define F_SEEK_CUR 2
#define F_SEEK_END 3

#define STDIN_FD 0
#define STDOUT_FD 1
#define STDERR_FD 2

#define P_NO_FILE_PERMISSION 0
#define P_WRITE_ONLY_FILE_PERMISSION 2
#define P_READ_ONLY_FILE_PERMISSION 4
#define P_READ_AND_EXECUTABLE_FILE_PERMISSION 5
#define P_READ_WRITE_FILE_PERMISSION 6
#define P_READ_WRITE_AND_EXECUTABLE_FILE_PERMISSION 5

typedef struct fat16_fs_st
{
    uint16_t *fat;
    size_t fat_size; // the total size of the fat
    uint16_t block_size;
    uint16_t blocks_in_fat;
    int fd;          // fd to the file of the FAT
    void *block_buf; // buffer of size block_size bytes
} fat16_fs;

typedef struct directory_entry_st
{
    char name[32];        // 32 bytes
    uint32_t size;        // 4
    uint16_t first_block; // 2
    uint8_t type;         // 1
    uint8_t perm;         // 1
    time_t mtime;         // 8
    char padding[16];     // 16
} directory_entry;        // 64 bytes in total!
_Static_assert(sizeof(directory_entry) == 64, "directory_entry must be 64 byes");

typedef struct global_fd_entry_st
{
    size_t ref_count;
    directory_entry *ptr_to_dir_entry; // an in-memory copy of the dir entry. This should be maintained so it always matches what is on disk
    uint16_t dir_entry_block_num;
    uint8_t dir_entry_idx;
    uint8_t write_locked; // mutex for whether this file is already being written to by another file. If the value is 0 the file is not write locked, 1 it opened with F_WRITE, and 2 it opened with F_APPEND
    uint32_t offset;
} global_fd_entry;

/**
 * @brief Mount the pennfat (fat16) filesystem from the file named fs_name
 * @param fs_name file name of the FAT in the host filesystem
 * @return int 0 on success, and an error code on error
 */
int mount(char *fs_name);

/**
 * @brief Unmount the pennfat (fat16) filesystem from the struct pointed to by ptr_to_fs.
 * This function will 0 out the struct on success (but may not on failure).
 *
 * @return int 0 on success, and an error code on error
 */
int unmount(void);

/**
 * @brief Check if the pennfat (fat16) filesystem is mounted
 * @return bool true if the pennfat (fat16) filesystem is mounted, false otherwise
 */
bool is_mounted(void);

/**
 * @brief Open a file
 * @param fname file name
 * @param mode mode to open the file with (i.e., F_READ, F_WRITE, F_APPEND)
 * @return int global file descriptor, or negative error code
 */
int k_open(const char *fname, int mode);

/**
 * @brief Close a file
 * @param fd global file descriptor to close
 * @return int 0 on success, or negative error code
 */
int k_close(int fd);

/**
 * @brief Read from a file
 * @param fd global file descriptor to read from
 * @param n number of bytes to read from the file
 * @param buf buffer to read the bytes into
 * @return int number of bytes read, or negative error code
 */
int k_read(int fd, int n, char *buf);

/**
 * @brief Seek to a position in a file
 * @param fd global file descriptor to seek in
 * @param offset offset to seek to
 * @param whence whence to seek from (i.e., F_SEEK_SET, F_SEEK_CUR, F_SEEK_END)
 * @return int64_t new offset, or negative error code
 * @note IMPORTANT: this function does not have the same signature as lseek(2) because
 * it returns a negative error code on error. On success, it returns the offset
 * which is guaranteed to fit in a uint32_t
 */
int64_t k_lseek(int fd, int offset, int whence);

/**
 * @brief Write to a file
 * @param fd global file descriptor to write to
 * @param str bytes to write to the file
 * @param n number of bytes to write to the file
 * @return int number of bytes written, or negative error code
 */
int k_write(int fd, const char *str, int n);

/**
 * @brief Remove (unlink) a file
 * @param fname file name
 * @return int 0 on success, or negative error code
 */
int k_unlink(const char *fname);

/**
 * @brief List the contents of a directory
 * @param filename directory to list the contents of
 * @return int 0 on success, or negative error code
 */
int k_ls(const char *filename);

/**
 * @brief Change the permissions of a file
 * @param fname file name
 * @param perm permissions to set the file to
 * @param mode mode to set the file to (i.e., F_READ, F_WRITE, F_APPEND)
 * @return int 0 on success, or negative error code
 */
int k_chmod(const char *fname, uint8_t perm, int mode);

/**
 * @brief Move a file from src to dest
 * @param src source file name
 * @param dest destination file name
 * @return int 0 on success, or negative error code
 */
int k_mv(const char *src, const char *dest);

/**
 * @brief Set the mode the global file descriptor is opened with
 * @param fd global file descriptor to set the mode of
 * @param mode mode to set the global file descriptor to (i.e., F_READ, F_WRITE, F_APPEND)
 * @return int 0 on success, or negative error code
 */
int k_setmode(int fd, int mode);

/**
 * @brief Get the mode the global file descriptor is opened with
 * @param fd global file descriptor to get the mode of
 * @return int mode of the global file descriptor, or negative error code
 */
int k_getmode(int fd);

/**
 * @brief Like dprintf but using pennfat and limited to 1023 characters
 * 
 * @param fd global file descriptor to write to
 * @param format format string
 * @param ... arguments to format string
 * @return int number of characters written, or negative error code
 */
int k_fprintf_short(int fd, const char *format, ...);

#endif // PENNFAT_FAT_H
