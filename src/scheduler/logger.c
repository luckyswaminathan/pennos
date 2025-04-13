#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <sys/types.h>
#include "logger.h"

static FILE* log_file = NULL;
static unsigned long current_ticks = 0;
static const char* level_strings[] = {
    "DEBUG",
    "INFO",
    "WARN",
    "ERROR"
};

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
    setlinebuf(log_file);
    current_ticks = 0;
}

void update_ticks(unsigned long ticks) {
    current_ticks = ticks;
}

void log_schedule(pid_t pid, int queue, const char* process_name) {
    if (!log_file) {
        log_file = stderr;
    }
    fprintf(log_file, "[%lu]\tSCHEDULE\t%d\t%d\t%s\n", 
            current_ticks, pid, queue, process_name);
}

void log_create(pid_t pid, int nice_value, const char* process_name) {
    if (!log_file) {
        log_file = stderr;
    }
    fprintf(log_file, "[%lu]\tCREATE\t%d\t%d\t%s\n", 
            current_ticks, pid, nice_value, process_name);
}

void log_signaled(pid_t pid, int nice_value, const char* process_name) {
    if (!log_file) {
        log_file = stderr;
    }
    fprintf(log_file, "[%lu]\tSIGNALED\t%d\t%d\t%s\n", 
            current_ticks, pid, nice_value, process_name);
}

void log_exited(pid_t pid, int nice_value, const char* process_name) {
    if (!log_file) {
        log_file = stderr;
    }
    fprintf(log_file, "[%lu]\tEXITED\t%d\t%d\t%s\n", 
            current_ticks, pid, nice_value, process_name);
}

void log_zombie(pid_t pid, int nice_value, const char* process_name) {
    if (!log_file) {
        log_file = stderr;
    }
    fprintf(log_file, "[%lu]\tZOMBIE\t%d\t%d\t%s\n", 
            current_ticks, pid, nice_value, process_name);
}

void log_orphan(pid_t pid, int nice_value, const char* process_name) {
    if (!log_file) {
        log_file = stderr;
    }
    fprintf(log_file, "[%lu]\tORPHAN\t%d\t%d\t%s\n", 
            current_ticks, pid, nice_value, process_name);
}

void log_waited(pid_t pid, int nice_value, const char* process_name) {
    if (!log_file) {
        log_file = stderr;
    }
    fprintf(log_file, "[%lu]\tWAITED\t%d\t%d\t%s\n", 
            current_ticks, pid, nice_value, process_name);
}

void log_nice(pid_t pid, int old_nice, int new_nice, const char* process_name) {
    if (!log_file) {
        log_file = stderr;
    }
    fprintf(log_file, "[%lu]\tNICE\t%d\t%d\t%d\t%s\n", 
            current_ticks, pid, old_nice, new_nice, process_name);
}

void log_blocked(pid_t pid, int nice_value, const char* process_name) {
    if (!log_file) {
        log_file = stderr;
    }
    fprintf(log_file, "[%lu]\tBLOCKED\t%d\t%d\t%s\n", 
            current_ticks, pid, nice_value, process_name);
}

void log_unblocked(pid_t pid, int nice_value, const char* process_name) {
    if (!log_file) {
        log_file = stderr;
    }
    fprintf(log_file, "[%lu]\tUNBLOCKED\t%d\t%d\t%s\n", 
            current_ticks, pid, nice_value, process_name);
}

void log_stopped(pid_t pid, int nice_value, const char* process_name) {
    if (!log_file) {
        log_file = stderr;
    }
    fprintf(log_file, "[%lu]\tSTOPPED\t%d\t%d\t%s\n", 
            current_ticks, pid, nice_value, process_name);
}

void log_continued(pid_t pid, int nice_value, const char* process_name) {
    if (!log_file) {
        log_file = stderr;
    }
    fprintf(log_file, "[%lu]\tCONTINUED\t%d\t%d\t%s\n", 
            current_ticks, pid, nice_value, process_name);
}

// Generic logging function for custom events
void log_custom(const char* operation, const char* format, ...) {
    if (!log_file) {
        log_file = stderr;
    }
    
    fprintf(log_file, "[%lu]\t%s\t", current_ticks, operation);
    
    va_list args;
    va_start(args, format);
    vfprintf(log_file, format, args);
    va_end(args);
    
    fprintf(log_file, "\n");
}

// Legacy log function for backward compatibility
void log_message(log_level_t level, const char* format, ...) {
    if (!log_file) {
        log_file = stderr;
    }
    
    // Use INTERNAL as the operation for legacy log messages
    fprintf(log_file, "[%lu]\tINTERNAL\t[%s] ", 
            current_ticks, level_strings[level]);
    
    va_list args;
    va_start(args, format);
    vfprintf(log_file, format, args);
    va_end(args);
    
    // Add newline if not already present
    if (format[strlen(format) - 1] != '\n') {
        fprintf(log_file, "\n");
    }
}
