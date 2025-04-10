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
    proc->thread = (spthread_t*)exiting_malloc(sizeof(spthread_t));
    if (spthread_create(proc->thread, NULL, func, argv) != 0) {
        LOG_ERROR("Failed to create thread for process %d", proc->pid);
        return -1;
    }
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
                spthread_join(*proc->thread, (void**)wstatus);
                k_proc_cleanup(proc);
                return pid;
            }

        }
        LOG_INFO("Process %d is not the one we're waiting for", proc->pid);
        proc = proc->next;
    }
    return -1;
}


int s_kill(pid_t pid) {
    pcb_t* proc = scheduler_state->processes.head;
    while (proc != NULL) {
        LOG_INFO("PROC PID %d", proc->pid);
        if (proc->pid == pid) {
            LOG_INFO("Killing process %d", proc->pid);
            int priority = proc->priority;
            if (priority == PRIORITY_HIGH) {
                linked_list_remove(&scheduler_state->priority_high, proc);
            } else if (priority == PRIORITY_MEDIUM) {
                linked_list_remove(&scheduler_state->priority_medium, proc);
            } else if (priority == PRIORITY_LOW) {
                linked_list_remove(&scheduler_state->priority_low, proc);
            }
            proc->state = PROCESS_TERMINATED;
            linked_list_push_tail(&scheduler_state->terminated_processes, proc);
            spthread_cancel(*proc->thread);
            return 0;
        }
        proc = proc->next;
    }
    return -1;
}

int s_nice(pid_t pid, int priority) {
    pcb_t* proc = scheduler_state->processes.head;
    while (proc != NULL) {
        LOG_INFO("PROC PID %d", proc->pid);
        if (proc->pid == pid) {
            LOG_INFO("Setting priority of process %d to %d", proc->pid, priority);
            if (proc->priority != priority) {
                if (priority == PRIORITY_HIGH) {
                    if (proc->priority == PRIORITY_MEDIUM) {
                        linked_list_remove(&scheduler_state->priority_medium, proc);
                    } else if (proc->priority == PRIORITY_LOW) {
                        linked_list_remove(&scheduler_state->priority_low, proc);
                    }
                    proc->priority = priority;
                    linked_list_push_tail(&scheduler_state->priority_high, proc);
                } else if (priority == PRIORITY_MEDIUM) {
                    if (proc->priority == PRIORITY_HIGH) {
                        linked_list_remove(&scheduler_state->priority_high, proc);
                    } else if (proc->priority == PRIORITY_LOW) {
                        linked_list_remove(&scheduler_state->priority_low, proc);
                    }
                    proc->priority = priority;
                    linked_list_push_tail(&scheduler_state->priority_medium, proc);
                } else if (priority == PRIORITY_LOW) {
                    if (proc->priority == PRIORITY_HIGH) {
                        linked_list_remove(&scheduler_state->priority_high, proc);
                    } else if (proc->priority == PRIORITY_MEDIUM) {
                        linked_list_remove(&scheduler_state->priority_medium, proc);
                    }
                    proc->priority = priority;
                    linked_list_push_tail(&scheduler_state->priority_low, proc);
                }
            }
            return 0;
        }
        proc = proc->next;
    }
    return -1;
}
    
// void s_exit(void) {
//     if (scheduler_state->curr != NULL) {
//         terminate_process(scheduler_state->curr);
//     }
//     run_scheduler();
// } 


// void s_sleep(unsigned int ticks) {
//     put_process_to_sleep(scheduler_state->curr, ticks);
//     run_scheduler();
// }
    
