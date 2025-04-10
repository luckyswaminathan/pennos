#ifndef PENNSHELL_EXITING_ALLOC_H
#define PENNSHELL_EXITING_ALLOC_H

#include <stdlib.h>

/*
 * @brief A wrapper around malloc that calls perror and exits if malloc returns NULL.
 * @return The pointer return by this function can always be trusted to be
 * valid.
 */
void *exiting_malloc(size_t size);

#endif // PENNSHELL_EXITING_ALLOC_H
