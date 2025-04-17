#include "scheduler.h"
#include "kernel.h"
#include "logger.h"
#include "../../lib/exiting_alloc.h"
#include "../../lib/linked_list.h"
#include "spthread.h"


pid_t s_spawn(void* (*func)(void*), void* arg);
pid_t s_waitpid(pid_t pid, int* wstatus, bool nohang);
int s_kill(pid_t pid);
int s_nice(pid_t pid, int priority);
int s_stop(pid_t pid);
int s_cont(pid_t pid);
void s_exit(int status);
