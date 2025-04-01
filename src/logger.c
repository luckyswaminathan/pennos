#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdarg.h>
#include <string.h>
#include "logger.h"

static FILE* log_file = NULL;
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
        log_file = fopen(file_path, "a");
        if (!log_file) {
            log_file = stderr;
        }
    } else {
        log_file = stderr;
    }
    setlinebuf(log_file);
}

void log_message(log_level_t level, const char* format, ...) {
    if (!log_file) {
        log_file = stderr;
    }

    time_t now;
    time(&now);
    struct tm* tm_info = localtime(&now);
    char timestamp[26];
    strftime(timestamp, 26, "%Y-%m-%d %H:%M:%S", tm_info);

    fprintf(log_file, "[%s] [%s] ", timestamp, level_strings[level]);

    va_list args;
    va_start(args, format);
    vfprintf(log_file, format, args);
    va_end(args);

    if (format[strlen(format) - 1] != '\n') {
        fprintf(log_file, "\n");
    }
}
