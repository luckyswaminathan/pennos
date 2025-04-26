#ifndef PENNOS_ERRNO_H
#define PENNOS_ERRNO_H

char* u_strerror(int errno);

void u_perror(const char *s);

#endif // PENNOS_ERRNO_H
