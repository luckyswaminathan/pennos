#include "./exiting_alloc.h"
#include <unistd.h>

// TODO: don't use this
void *exiting_malloc(size_t size)
{
  void *memory = malloc(size);
  if (memory == NULL)
  {
    exit(EXIT_FAILURE);
  }
  return memory;
}
