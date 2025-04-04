#include "scheduler.h"
#include "kernel.h"
#include "logger.h"
#include "../shell/exiting_alloc.h"
#include "../lib/linked_list.h"
#include "spthread.h"


pid_t s_spawn(void* (*func)(void*), char *argv[], int fd0, int fd1);
pid_t s_waitpid(pid_t pid, int* wstatus, bool nohang);
int s_kill(pid_t pid);
int s_nice(pid_t pid, int priority);