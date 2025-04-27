#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h> // for vsnprintf

#include "src/pennfat/fat.h"
#include "src/scheduler/kernel.h"
#include "src/utils/error_codes.h"
#include "sys.h"

#define PROCESS_FD_TABLE_ENTRY_NOT_FOUND_SENTINEL 0xFFFF

// TODO: are we allowed to do this as a syscall?
int find_empty_spot_in_process_fd_table(void)
{
    pcb_t *current_process = k_get_current_process();
    for (uint32_t i = 0; i < PROCESS_FD_TABLE_SIZE; i++)
    {
        // check if the entry already exists (requires ref_count > 0)
        if (!current_process->process_fd_table[i].in_use)
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

    // find an empty spot in the process fd table
    uint32_t empty_spot = find_empty_spot_in_process_fd_table();
    if (empty_spot == PROCESS_FD_TABLE_ENTRY_NOT_FOUND_SENTINEL)
    {
        return ES_PROCESS_FILE_TABLE_FULL;
    }

    // update the process fd table
    pcb_t *current_process = k_get_current_process();
    current_process->process_fd_table[empty_spot].global_fd = global_fd;
    current_process->process_fd_table[empty_spot].in_use = true;
    current_process->process_fd_table[empty_spot].offset = 0;
    current_process->process_fd_table[empty_spot].mode = mode;
    return empty_spot;
}

#define ES_READ_UNKNOWN_FD -101
#define ES_READ_NO_TERMINAL_CONTROL -106

int s_read(int fd, int n, char *buf)
{
    pcb_t *current_process = k_get_current_process();
    if (fd >= PROCESS_FD_TABLE_SIZE || !current_process->process_fd_table[fd].in_use)
    {
        return ES_READ_UNKNOWN_FD;
    }

    int global_fd = current_process->process_fd_table[fd].global_fd;
    if (global_fd == STDIN_FD && k_tcgetpid() != current_process->pid) {        
        k_fprintf_short(STDERR_FD, "[WARN]: process %d tried to read from terminal STDIN without terminal control\n", current_process->pid);
        s_kill(current_process->pid, P_SIGSTOP);
    }

    // set the offset in the global fd table
    k_lseek(current_process->process_fd_table[fd].global_fd, current_process->process_fd_table[fd].offset, F_SEEK_SET);
    int bytes_read = k_read(current_process->process_fd_table[fd].global_fd, n, buf);
    current_process->process_fd_table[fd].offset += bytes_read;
    return bytes_read;
}

#define ES_WRITE_UNKNOWN_FD -102
#define ES_SEEK_ERROR -103
#define ES_SETMODE_ERROR -104
#define ES_WRITE_NO_TERMINAL_CONTROL -105
int s_write(int fd, const char *str, int n)
{
    pcb_t *current_process = k_get_current_process();
    if (fd >= PROCESS_FD_TABLE_SIZE || !current_process->process_fd_table[fd].in_use)
    {
        return ES_WRITE_UNKNOWN_FD;
    }

    int global_fd = current_process->process_fd_table[fd].global_fd;
    if (global_fd == STDIN_FD && k_tcgetpid() != current_process->pid) {
        k_fprintf_short(STDERR_FD, "[WARN]: process %d tried to write to terminal STDIN without terminal control\n", current_process->pid);
        s_kill(current_process->pid, P_SIGSTOP);
    }

    // set the offset in the global fd table
    int seek_status = k_lseek(current_process->process_fd_table[fd].global_fd, current_process->process_fd_table[fd].offset, F_SEEK_SET);
    if (seek_status != EK_LSEEK_SPECIAL_FD && seek_status < 0) // it's OK if the fd is a special fd
    {
        return ES_SEEK_ERROR;
    }

    int old_mode = k_getmode(current_process->process_fd_table[fd].global_fd);
    int setmode_status = k_setmode(current_process->process_fd_table[fd].global_fd, current_process->process_fd_table[fd].mode);
    if (setmode_status != 0)
    {
        return ES_SETMODE_ERROR;
    }

    int bytes_written = k_write(current_process->process_fd_table[fd].global_fd, str, n);

    if (k_setmode(current_process->process_fd_table[fd].global_fd, old_mode) != 0)
    {
        return ES_SETMODE_ERROR;
    }

    current_process->process_fd_table[fd].offset += bytes_written;
    return bytes_written;
}

#define ES_CLOSE_UNKNOWN_FD -103

int s_close(int fd)
{
    pcb_t *current_process = k_get_current_process();
    if (fd >= PROCESS_FD_TABLE_SIZE || !current_process->process_fd_table[fd].in_use)
    {
        return ES_CLOSE_UNKNOWN_FD;
    }
    // close the global fd
    k_close(current_process->process_fd_table[fd].global_fd);
    current_process->process_fd_table[fd].in_use = false;
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

int s_chmod(const char *fname, uint8_t perm, int mode)
{
    return k_chmod(fname, perm, mode);
}

int s_mv(const char *src, const char *dest)
{
    return k_mv(src, dest);
}

char S_FPRINTF_SHORT_BUF[1024];

int s_fprintf_short(int fd, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    int status = vsnprintf(S_FPRINTF_SHORT_BUF, sizeof(S_FPRINTF_SHORT_BUF), format, args);
    va_end(args);
    if (status < 0) {
        return E_STR_FORMAT_FAILED;
    }
    if (status >= sizeof(S_FPRINTF_SHORT_BUF)) {
        return E_STR_TOO_LONG_FOR_FPRINTF_BUF;
    }
    return s_write(fd, S_FPRINTF_SHORT_BUF, strlen(S_FPRINTF_SHORT_BUF));
}
