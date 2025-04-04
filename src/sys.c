#include "scheduler.h"
#include "kernel.h"
#include "logger.h"
#include "../shell/exiting_alloc.h"
#include "../lib/linked_list.h"
#include "spthread.h"




pid_t s_spawn(void* (*func)(void*), char *argv[], int fd0, int fd1) {
    pcb_t* proc = k_proc_create(scheduler_state->init);
    proc->fd0 = fd0;
    proc->fd1 = fd1;
    proc->argv = argv;
    spthread_create(&proc->thread, NULL, func, NULL);
    return proc->pid;
}

pid_t s_waitpid(pid_t pid, int* wstatus, bool nohang) {
    pcb_t* proc = scheduler_state->processes.head;
    while (proc != NULL) {
        if (proc->pid == pid) {
            LOG_INFO("Waiting for process %d", proc->pid);
            
            if (proc->state == PROCESS_TERMINATED) {
                k_proc_cleanup(proc);
                return pid;
            }
            if (nohang) {
                return -1;
            } else {
                spthread_join(proc->thread, (void**)wstatus);
                k_proc_cleanup(proc);
                return pid;
            }
        }
        proc = proc->next;
    }
    return -1;
}

int s_kill(pid_t pid) {
    pcb_t* proc = scheduler_state->processes.head;
    while (proc != NULL) {
        if (proc->pid == pid) {
            LOG_INFO("Killing process %d", proc->pid);
            proc->state = PROCESS_TERMINATED;
            return 0;
        }
        proc = proc->next;
    }
    return -1;
}

int s_nice(pid_t pid, int priority) {
    pcb_t* proc = scheduler_state->processes.head;
    while (proc != NULL) {
        if (proc->pid == pid) {
            LOG_INFO("Setting priority of process %d to %d", proc->pid, priority);
            if (proc->priority != priority) {
                linked_list_remove(&scheduler_state->priority_medium, proc);
                if (priority == PRIORITY_HIGH) {
                    linked_list_push_tail(&scheduler_state->priority_high, proc);
                } else if (priority == PRIORITY_MEDIUM) {
                    linked_list_push_tail(&scheduler_state->priority_medium, proc);
                } else if (priority == PRIORITY_LOW) {
                    linked_list_push_tail(&scheduler_state->priority_low, proc);
                }
            }
            proc->priority = priority;
            
            return 0;
        }
        proc = proc->next;
    }
    return -1;
}
    