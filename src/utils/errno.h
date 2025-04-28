#ifndef PENNOS_ERRNO_H
#define PENNOS_ERRNO_H

#include "src/utils/error_codes.h"

/**
 * @brief Convert an error code to a string
 * @param errno error code
 * @return char* string representation of the error code. Note that this is a static string allocated to an internal buffer. 
 * It should not be freed nor modified, and should be copied if the caller needs to keep it.
 */
char* u_strerror(int errno);

/**
 * @brief Print an error message to stderr  
 * @param s error message
 */
void u_perror(const char *s);

#endif // PENNOS_ERRNO_H
