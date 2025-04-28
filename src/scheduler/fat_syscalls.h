#include "src/pennfat/fat_constants.h"

/**
 * @brief Open a file
 * @param fname file name
 * @param mode mode to open the file with (i.e., F_READ, F_WRITE, F_APPEND)
 * @return int process-level file descriptor, or negative error code
 */
int s_open(const char *fname, int mode);

/**
 * @brief Read from a file
 * @param fd process-level file descriptor to read from
 * @param n number of bytes to read from the file
 * @param buf buffer to read the bytes into
 * @return int number of bytes read, or negative error code
 */
int s_read(int fd, int n, char *buf);

/**
 * @brief Write to a file
 * @param fd process-level file descriptor to write to
 * @param str string to write to the file
 * @param n number of bytes to write to the file
 * @return int number of bytes written, or negative error code
 */
int s_write(int fd, const char *str, int n);

/**
 * @brief Close a file 
 * @param fd process-level file descriptor to close
 * @return int 0 on success, or negative error code
 */
int s_close(int fd);

/**
 * @brief Remove (unlink) a file
 * @param fname file name
 * @return int 0 on success, or negative error code
 */
int s_unlink(const char *fname);

/**
 * @brief List the contents of a directory  
 * @param filename directory to list the contents of
 * @return int 0 on success, or negative error code
 */
int s_ls(const char *filename);

/**
 * @brief Change the permissions of a file
 * @param fname file name
 * @param perm permissions to set the file to
 * @param mode mode to set the file to (i.e., F_READ, F_WRITE, F_APPEND)
 * @return int 0 on success, or negative error code
 */
int s_chmod(const char *fname, uint8_t perm, int mode);

/**
 * @brief Move a file from src to dest
 * @param src source file name
 * @param dest destination file name
 * @return int 0 on success, or negative error code
 */
int s_mv(const char *src, const char *dest);

/**
 * @brief Like dprintf but using pennfat and limited to 1023 characters
 * @param fd process-level file descriptor to write to
 * @param format format string
 * @param ... arguments to format string
 * @return int number of characters written, or negative error code
 */
int s_fprintf_short(int fd, const char *format, ...);
