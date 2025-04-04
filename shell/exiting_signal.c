#include "./exiting_signal.h"
#include <signal.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>

void exiting_set_signal_handler(int sig, void (*handler)(int))
{
  struct sigaction new_sigaction = {0};
  sigemptyset(&new_sigaction.sa_mask);
  new_sigaction.sa_handler = handler;
  new_sigaction.sa_flags = SA_RESTART;
  int sigaction_result = sigaction(sig, &new_sigaction, NULL);
  if (sigaction_result == -1)
  {
    perror("Failed to set signal handler");
    exit(EXIT_FAILURE);
  }
}
