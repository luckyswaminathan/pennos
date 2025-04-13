#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>

/**
 * Initialize the logger
 * 
 * @param log_file Path to the log file, or NULL to use stderr
 */
void init_logger(const char* log_file);

/**
 * Update the current tick count
 * 
 * @param ticks The current tick count
 */
void update_ticks(unsigned long ticks);

/**
 * Log a schedule event
 * Format: [ticks] SCHEDULE PID QUEUE PROCESS_NAME
 */
void log_schedule(pid_t pid, int queue, const char* process_name);

/**
 * Log a process creation event
 * Format: [ticks] CREATE PID NICE_VALUE PROCESS_NAME
 */
void log_create(pid_t pid, int nice_value, const char* process_name);

/**
 * Log a process signaled event
 * Format: [ticks] SIGNALED PID NICE_VALUE PROCESS_NAME
 */
void log_signaled(pid_t pid, int nice_value, const char* process_name);

/**
 * Log a process exit event
 * Format: [ticks] EXITED PID NICE_VALUE PROCESS_NAME
 */
void log_exited(pid_t pid, int nice_value, const char* process_name);

/**
 * Log a process becoming zombie event
 * Format: [ticks] ZOMBIE PID NICE_VALUE PROCESS_NAME
 */
void log_zombie(pid_t pid, int nice_value, const char* process_name);

/**
 * Log a process becoming orphan event
 * Format: [ticks] ORPHAN PID NICE_VALUE PROCESS_NAME
 */
void log_orphan(pid_t pid, int nice_value, const char* process_name);

/**
 * Log a process being waited on event
 * Format: [ticks] WAITED PID NICE_VALUE PROCESS_NAME
 */
void log_waited(pid_t pid, int nice_value, const char* process_name);

/**
 * Log a nice value change event
 * Format: [ticks] NICE PID OLD_NICE_VALUE NEW_NICE_VALUE PROCESS_NAME
 */
void log_nice(pid_t pid, int old_nice, int new_nice, const char* process_name);

/**
 * Log a process blocked event
 * Format: [ticks] BLOCKED PID NICE_VALUE PROCESS_NAME
 */
void log_blocked(pid_t pid, int nice_value, const char* process_name);

/**
 * Log a process unblocked event
 * Format: [ticks] UNBLOCKED PID NICE_VALUE PROCESS_NAME
 */
void log_unblocked(pid_t pid, int nice_value, const char* process_name);

/**
 * Log a process stopped event
 * Format: [ticks] STOPPED PID NICE_VALUE PROCESS_NAME
 */
void log_stopped(pid_t pid, int nice_value, const char* process_name);

/**
 * Log a process continued event
 * Format: [ticks] CONTINUED PID NICE_VALUE PROCESS_NAME
 */
void log_continued(pid_t pid, int nice_value, const char* process_name);

/**
 * Log a custom event
 * Format: [ticks] OPERATION args...
 * 
 * @param operation The operation name to log
 * @param format Format string for the arguments
 */
void log_custom(const char* operation, const char* format, ...);

/* 
 * =========================================
 * Backward compatibility with old log macros
 * =========================================
 */

typedef enum {
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR
} log_level_t;

// Legacy log function for backward compatibility
void log_message(log_level_t level, const char* format, ...);

// Keep the old macros for backward compatibility
#define LOG_DEBUG(...) log_message(LOG_DEBUG, __VA_ARGS__)
#define LOG_INFO(...) log_message(LOG_INFO, __VA_ARGS__)
#define LOG_WARN(...) log_message(LOG_WARN, __VA_ARGS__)
#define LOG_ERROR(...) log_message(LOG_ERROR, __VA_ARGS__)

/**
 * Logs a critical error message and terminates the program
 * @param msg The error message
 */
#define PANIC(msg) do { \
    log_message(LOG_ERROR, "PANIC: %s", (msg)); \
    fprintf(stderr, "PANIC: %s\n", (msg)); \
    exit(EXIT_FAILURE); \
} while (0)

#endif
