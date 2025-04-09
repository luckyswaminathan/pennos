#ifndef PENNSHELL_PRINT_H
#define PENNSHELL_PRINT_H

#include <unistd.h>

/*
 * @brief Print a string to stderr.
 * @param str The string to print.
 * @return The number of bytes written to stderr. If the write fails, calls perror
 * and exits with a non-zero exit code.
 */
size_t print_stderr(char *str);

#endif // PENNSHELL_PRINT_H
