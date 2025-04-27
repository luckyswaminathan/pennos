#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include "logger.h"
#include "scheduler.h"
#define LOG_BUF_SIZE 256

static FILE* log_file = NULL;

void init_logger(const char* file_path) {
    if (log_file && log_file != stderr) {
        fclose(log_file);
    }
    if (file_path) {
        log_file = fopen(file_path, "w");
        if (!log_file) {
            log_file = stderr;
        }
    } else {
        log_file = stderr;
    }
}

void log_schedule(pid_t pid, int queue, const char* process_name) {
    if (!log_file) {
        log_file = stderr;
    }
    char buf[LOG_BUF_SIZE];
    int fd = fileno(log_file);
    int len = snprintf(buf, LOG_BUF_SIZE, "[%d]\tSCHEDULE \t%d\t%d\t%s\n",
                       k_get_quantum(), pid, queue, process_name);
    if (len > 0) {
        write(fd, buf, len);
    }
}

void log_create(pid_t pid, int nice_value, const char* process_name) {
    if (!log_file) {
        log_file = stderr;
    }
    char buf[LOG_BUF_SIZE];
    int fd = fileno(log_file);
    int len = snprintf(buf, LOG_BUF_SIZE, "[%d]\tCREATE   \t%d\t%d\t%s\n",
                       k_get_quantum(), pid, nice_value, process_name);
    if (len > 0) {
        write(fd, buf, len);
    }
}

void log_signaled(pid_t pid, int nice_value, const char* process_name) {
    if (!log_file) {
        log_file = stderr;
    }
    char buf[LOG_BUF_SIZE];
    int fd = fileno(log_file);
    int len = snprintf(buf, LOG_BUF_SIZE, "[%d]\tSIGNALED \t%d\t%d\t%s\n",
                       k_get_quantum(), pid, nice_value, process_name);
    if (len > 0) {
        write(fd, buf, len);
    }
}

void log_exited(pid_t pid, int nice_value, const char* process_name) {
    if (!log_file) {
        log_file = stderr;
    }
    char buf[LOG_BUF_SIZE];
    int fd = fileno(log_file);
    int len = snprintf(buf, LOG_BUF_SIZE, "[%d]\tEXITED   \t%d\t%d\t%s\n",
                       k_get_quantum(), pid, nice_value, process_name);
    if (len > 0) {
        write(fd, buf, len);
    }
}

void log_zombie(pid_t pid, int nice_value, const char* process_name) {
    if (!log_file) {
        log_file = stderr;
    }
    char buf[LOG_BUF_SIZE];
    int fd = fileno(log_file);
    int len = snprintf(buf, LOG_BUF_SIZE, "[%d]\tZOMBIE   \t%d\t%d\t%s\n",
                       k_get_quantum(), pid, nice_value, process_name);
    if (len > 0) {
        write(fd, buf, len);
    }
}

void log_orphan(pid_t pid, int nice_value, const char* process_name) {
    if (!log_file) {
        log_file = stderr;
    }
    char buf[LOG_BUF_SIZE];
    int fd = fileno(log_file);
    int len = snprintf(buf, LOG_BUF_SIZE, "[%d]\tORPHAN   \t%d\t%d\t%s\n",
                       k_get_quantum(), pid, nice_value, process_name);
    if (len > 0) {
        write(fd, buf, len);
    }
}

/**
 * @brief Log a process that has been waited on
 * 
 * @param pid The PID of the process that has been waited on
 * @param nice_value The nice value of the process that has been waited on
 * @param process_name The name of the process that has been waited on
 */
void log_waited(pid_t pid, int nice_value, const char* process_name) {
    if (!log_file) {
        log_file = stderr;
    }
    char buf[LOG_BUF_SIZE];
    int fd = fileno(log_file);
    int len = snprintf(buf, LOG_BUF_SIZE, "[%d]\tWAITED   \t%d\t%d\t%s\n",
                       k_get_quantum(), pid, nice_value, process_name);
    if (len > 0) {
        write(fd, buf, len);
    }
}

void log_nice(pid_t pid, int old_nice, int new_nice, const char* process_name) {
    if (!log_file) {
        log_file = stderr;
    }
    char buf[LOG_BUF_SIZE];
    int fd = fileno(log_file);
    int len = snprintf(buf, LOG_BUF_SIZE, "[%d]\tNICE     \t%d\t%d\t%d\t%s\n",
                       k_get_quantum(), pid, old_nice, new_nice, process_name);
    if (len > 0) {
        write(fd, buf, len);
    }
}

void log_blocked(pid_t pid, int nice_value, const char* process_name) {
    if (!log_file) {
        log_file = stderr;
    }
    char buf[LOG_BUF_SIZE];
    int fd = fileno(log_file);
    int len = snprintf(buf, LOG_BUF_SIZE, "[%d]\tBLOCKED  \t%d\t%d\t%s\n",
                       k_get_quantum(), pid, nice_value, process_name);
    if (len > 0) {
        write(fd, buf, len);
    }
}

void log_unblocked(pid_t pid, int nice_value, const char* process_name) {
    if (!log_file) {
        log_file = stderr;
    }
    char buf[LOG_BUF_SIZE];
    int fd = fileno(log_file);
    int len = snprintf(buf, LOG_BUF_SIZE, "[%d]\tUNBLOCKED\t%d\t%d\t%s\n",
                       k_get_quantum(), pid, nice_value, process_name);
    if (len > 0) {
        write(fd, buf, len);
    }
}

void log_sleep(pid_t pid, int nice_value, const char* process_name) {
    if (!log_file) {
        log_file = stderr;
    }
    char buf[LOG_BUF_SIZE];
    int fd = fileno(log_file);
    int len = snprintf(buf, LOG_BUF_SIZE, "[%d]\tSLEEPING\t%d\t%d\t%s\n",
                       k_get_quantum(), pid, nice_value, process_name);
    if (len > 0) {
        write(fd, buf, len);
    }
}

void log_stopped(pid_t pid, int nice_value, const char* process_name) {
    if (!log_file) {
        log_file = stderr;
    }
    char buf[LOG_BUF_SIZE];
    int fd = fileno(log_file);
    int len = snprintf(buf, LOG_BUF_SIZE, "[%d]\tSTOPPED  \t%d\t%d\t%s\n",
                       k_get_quantum(), pid, nice_value, process_name);
    if (len > 0) {
        write(fd, buf, len);
    }
}

void log_continued(pid_t pid, int nice_value, const char* process_name) {
    if (!log_file) {
        log_file = stderr;
    }
    char buf[LOG_BUF_SIZE];
    int fd = fileno(log_file);
    int len = snprintf(buf, LOG_BUF_SIZE, "[%d]\tCONTINUED\t%d\t%d\t%s\n",
                       k_get_quantum(), pid, nice_value, process_name);
    if (len > 0) {
        write(fd, buf, len);
    }
}

// Generic logging function for custom events
void log_custom(const char* operation, const char* format, ...) {
    if (!log_file) {
        log_file = stderr;
    }

    char buf[LOG_BUF_SIZE];
    int fd = fileno(log_file);
    int offset = 0;

    // Format the initial part
    offset = snprintf(buf, LOG_BUF_SIZE, "[%d]\t%s\t", k_get_quantum(), operation);
    if (offset < 0 || offset >= LOG_BUF_SIZE) return; // Handle snprintf error or truncation

    // Format the variable part
    va_list args;
    va_start(args, format);
    int len_va = vsnprintf(buf + offset, LOG_BUF_SIZE - offset, format, args);
    va_end(args);
    if (len_va < 0) return; // Handle vsnprintf error
    offset += len_va;

    // Ensure string is written fully or truncated safely
    int write_len = offset;
    if (offset >= LOG_BUF_SIZE) {
       write_len = LOG_BUF_SIZE -1; // Max possible write length if truncated
    }

    // Add newline if space allows and it wasn't truncated mid-write
    if (write_len < LOG_BUF_SIZE - 1) {
        buf[write_len++] = '\n';
        // buf[write_len] = '\0'; // Null terminate only needed if using string funcs later
    } else {
        // Overwrite the last char with newline if truncated
         buf[LOG_BUF_SIZE - 2] = '\n';
         // buf[LOG_BUF_SIZE - 1] = '\0';
         write_len = LOG_BUF_SIZE -1; // Write up to newline
    }

    // Write the final buffer
     write(fd, buf, write_len); // Write the determined length

}

// Legacy log function for backward compatibility
void log_message(log_level_t level, const char* format, ...) {
    if (!log_file) {
        log_file = stderr;
    }

    char buf[LOG_BUF_SIZE];
    int fd = fileno(log_file);
    int len = 0;

    // Use INTERNAL as the operation for legacy log messages (original fprintf commented out)
    // offset = snprintf(buf, LOG_BUF_SIZE, "[%d]\tINTERNAL\t[%s] ",
    //         current_ticks, level_strings[level]);
    // if (offset < 0 || offset >= LOG_BUF_SIZE) return;

    va_list args;
    va_start(args, format);
    // Format the main message
    len = vsnprintf(buf, LOG_BUF_SIZE, format, args);
    va_end(args);

    if (len < 0) return; // Handle vsnprintf error

    // Determine actual length considering potential truncation
    int write_len = len;
    if (len >= LOG_BUF_SIZE) {
        write_len = LOG_BUF_SIZE - 1;
    }
    // No need for explicit null termination for write()

    // Write the formatted message
    write(fd, buf, write_len);

    // Check original format string to decide if newline is needed
    size_t format_len = strlen(format);
    int needs_newline = (format_len == 0 || format[format_len - 1] != '\n');

    // Add newline if needed and it wasn't truncated
    if (needs_newline && write_len == len) {
        write(fd, "\n", 1);
    }
    // If it was truncated (write_len != len), we don't add an extra newline
    // as the original content (including potential newline) was cut off.
}
