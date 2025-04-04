// TODO: we should just use fprintf here since it's a better impl (maybe wrap it variadically to always use stderr)
#include "./print.h"
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

size_t print_stderr(char *str)
{
  ssize_t ret = write(STDERR_FILENO, str, strlen(str));
  if (ret < 0) {
    perror("Failed to print to stderr");
    exit(EXIT_FAILURE);
  }
  return ret;
}
