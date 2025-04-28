#ifndef PENNOS_ERRNO_H
#define PENNOS_ERRNO_H

#include "src/utils/error_codes.h"

char* u_strerror(int errno);

void u_perror(const char *s);

#endif // PENNOS_ERRNO_H
