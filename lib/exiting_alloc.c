#include "./exiting_alloc.h"
#include <unistd.h>
#include <stdio.h>

void *exiting_malloc(size_t size)
{
  void *memory = malloc(size);
  if (memory == NULL)
  {
    perror("Failed to allocate memory");
    exit(EXIT_FAILURE);
  }
  return memory;
}
