#include <stdint.h>
#include "src/pennfat/fat.h"

#define PROCESS_FD_TABLE_SIZE 1024
#define PROCESS_FD_TABLE_ENTRY_NOT_FOUND_SENTINEL 0xFFFF

typedef struct process_fd_entry_st
{
    uint16_t global_fd; // file descriptor
    uint32_t offset;
    uint8_t mode; // F_READ, F_WRITE, F_APPEND
    bool in_use;
} process_fd_entry;

process_fd_entry process_fd_table[PROCESS_FD_TABLE_SIZE] = {0}; // all in_use are false

int find_empty_spot_in_process_fd_table(void)
{
    for (uint32_t i = 0; i < PROCESS_FD_TABLE_SIZE; i++)
    {
        // check if the entry already exists (requires ref_count > 0)
        if (!process_fd_table[i].in_use)
        {
            return i;
        }
    }
    return PROCESS_FD_TABLE_ENTRY_NOT_FOUND_SENTINEL;
}

int find_global_fd_in_process_fd_table(int global_fd)
{
    for (uint32_t i = 0; i < PROCESS_FD_TABLE_SIZE; i++)
    {
        if (process_fd_table[i].in_use && process_fd_table[i].global_fd == global_fd)
        {
            return i;
        }
    }
    return PROCESS_FD_TABLE_ENTRY_NOT_FOUND_SENTINEL;
}

#define ES_PROCESS_FILE_TABLE_FULL -100

int s_open(const char *fname, int mode)
{
    // try to open the file at the kernel level
    int global_fd = k_open(fname, mode);
    // TODO: some kind of error translation?
    if (global_fd < 0)
    {
        return global_fd;
    }

    // Check if the fd is already in the process fd table

    if (find_global_fd_in_process_fd_table(global_fd) != PROCESS_FD_TABLE_ENTRY_NOT_FOUND_SENTINEL)
    {
        return global_fd;
    }

    // find an empty spot in the process fd table
    uint32_t empty_spot = find_empty_spot_in_process_fd_table();
    if (empty_spot == PROCESS_FD_TABLE_ENTRY_NOT_FOUND_SENTINEL)
    {
        return ES_PROCESS_FILE_TABLE_FULL;
    }

    // update the process fd table
    process_fd_table[empty_spot].global_fd = global_fd;
    process_fd_table[empty_spot].in_use = true;
    process_fd_table[empty_spot].offset = 0;
    process_fd_table[empty_spot].mode = mode;
    return empty_spot;
}

#define ES_READ_UNKNOWN_FD -101

int s_read(int fd, int n, char *buf)
{
    if (fd >= PROCESS_FD_TABLE_SIZE || !process_fd_table[fd].in_use)
    {
        return ES_READ_UNKNOWN_FD;
    }

    // set the offset in the global fd table
    k_lseek(process_fd_table[fd].global_fd, process_fd_table[fd].offset, F_SEEK_SET);
    int bytes_read = k_read(process_fd_table[fd].global_fd, n, buf);
    process_fd_table[fd].offset += bytes_read;
    return bytes_read;
}

#define ES_WRITE_UNKNOWN_FD -102
#define ES_SEEK_ERROR -103
#define ES_SETMODE_ERROR -104
int s_write(int fd, const char *str, int n)
{
    if (fd >= PROCESS_FD_TABLE_SIZE || !process_fd_table[fd].in_use)
    {
        return ES_WRITE_UNKNOWN_FD;
    }

    // set the offset in the global fd table
    if (k_lseek(process_fd_table[fd].global_fd, process_fd_table[fd].offset, F_SEEK_SET) < 0)
    {
        return ES_SEEK_ERROR;
    }

    int old_mode = k_getmode(process_fd_table[fd].global_fd);
    if (k_setmode(process_fd_table[fd].global_fd, process_fd_table[fd].mode) != 0)
    {
        return ES_SETMODE_ERROR;
    }

    int bytes_written = k_write(process_fd_table[fd].global_fd, str, n);

    if (k_setmode(process_fd_table[fd].global_fd, old_mode) != 0)
    {
        return ES_SETMODE_ERROR;
    }

    process_fd_table[fd].offset += bytes_written;
    return bytes_written;
}

#define ES_CLOSE_UNKNOWN_FD -103

int s_close(int fd)
{
    if (fd >= PROCESS_FD_TABLE_SIZE || !process_fd_table[fd].in_use)
    {
        return ES_CLOSE_UNKNOWN_FD;
    }
    // close the global fd
    k_close(process_fd_table[fd].global_fd);
    process_fd_table[fd].in_use = false;
    return 0;
}

int s_unlink(const char *fname)
{
    return k_unlink(fname);
}

int s_ls(const char *filename)
{
    return k_ls(filename);
}

int s_chmod(const char *fname, uint8_t perm)
{
    return k_chmod(fname, perm);
}

int s_mv(const char *src, const char *dest)
{
    return k_mv(src, dest);
}