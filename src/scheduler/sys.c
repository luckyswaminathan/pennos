#include "scheduler.h"
#include "kernel.h"
#include "logger.h"
#include <errno.h>
#include "../../lib/exiting_alloc.h"
#include "../../lib/linked_list.h"
#include "spthread.h"




pid_t s_spawn(void* (*func)(void*), void* arg) {
    
    log_queue_state();
    pcb_t* proc = k_proc_create(scheduler_state->curr, arg);
    log_queue_state();
    proc->thread = (spthread_t*)exiting_malloc(sizeof(spthread_t));
    if (spthread_create(proc->thread, NULL, func, arg) != 0) {
        LOG_ERROR("Failed to create thread for process %d", proc->pid);
        return -1;
    }   
    return proc->pid;
}

pid_t s_waitpid(pid_t pid, int* wstatus, bool nohang) {
    LOG_INFO("s_waitpid called with pid %d, nohang %d", pid, nohang);
    
    // First check terminated processes queue
    pcb_t* proc = scheduler_state->terminated_processes.head;
    while (proc != NULL) {
        if (pid == -1 || proc->pid == pid) {
            // Found a terminated process we're looking for
            pid_t terminated_pid = proc->pid;
            LOG_INFO("Found terminated process %d", terminated_pid);
            
            // Remove from terminated queue
            linked_list_remove(&scheduler_state->terminated_processes, proc, priority_pointers.prev, priority_pointers.next);
            
            // Clean up the process
            k_proc_cleanup(proc);
            
            return terminated_pid;
        }
        proc = proc->priority_pointers.next;
    }
    
    // If not found in terminated queue, check running processes
    proc = scheduler_state->processes.head;
    while (proc != NULL) {
        if (pid == -1 || proc->pid == pid) {
            LOG_INFO("Found running process %d", proc->pid);
            
            if (proc->state == PROCESS_TERMINATED) {
                return proc->pid;
            }
            
            if (nohang) {
                return 0;
            } else {
                // Block until process completes
                spthread_join(*proc->thread, (void**)wstatus);
                return proc->pid;
            }
        }
        proc = proc->process_pointers.next;
    }
    
    return 0;
}


int s_kill(pid_t pid) {
    pcb_t* proc = scheduler_state->processes.head;
    while (proc != NULL) {
        if (proc->pid == pid) {
            LOG_INFO("Killing process %d", proc->pid);
            int priority = proc->priority;
            if (priority == PRIORITY_HIGH) {
                linked_list_remove(&scheduler_state->priority_high, proc, priority_pointers.prev, priority_pointers.next);
            } else if (priority == PRIORITY_MEDIUM) {
                linked_list_remove(&scheduler_state->priority_medium, proc, priority_pointers.prev, priority_pointers.next);
            } else if (priority == PRIORITY_LOW) {
                linked_list_remove(&scheduler_state->priority_low, proc, priority_pointers.prev, priority_pointers.next);
            }
            proc->state = PROCESS_TERMINATED;
            linked_list_push_tail(&scheduler_state->terminated_processes, proc, priority_pointers.prev, priority_pointers.next);
            spthread_cancel(*proc->thread);
            return 0;
        }
        proc = proc->process_pointers.next;
    }
    return -1;
}

int s_nice(pid_t pid, int priority) {
    pcb_t* proc = scheduler_state->processes.head;
    while (proc != NULL) {
        if (proc->pid == pid) {
            LOG_INFO("Setting priority of process %d to %d", proc->pid, priority);
            if (proc->priority != priority) {
                if (priority == PRIORITY_HIGH) {
                    if (proc->priority == PRIORITY_MEDIUM) {
                        linked_list_remove(&scheduler_state->priority_medium, proc, priority_pointers.prev, priority_pointers.next);
                    } else if (proc->priority == PRIORITY_LOW) {
                        linked_list_remove(&scheduler_state->priority_low, proc, priority_pointers.prev, priority_pointers.next);
                    }
                    proc->priority = priority;
                    linked_list_push_tail(&scheduler_state->priority_high, proc, priority_pointers.prev, priority_pointers.next);
                } else if (priority == PRIORITY_MEDIUM) {
                    if (proc->priority == PRIORITY_HIGH) {
                        linked_list_remove(&scheduler_state->priority_high, proc, priority_pointers.prev, priority_pointers.next);
                    } else if (proc->priority == PRIORITY_LOW) {
                        linked_list_remove(&scheduler_state->priority_low, proc, priority_pointers.prev, priority_pointers.next);
                    }
                    proc->priority = priority;
                    linked_list_push_tail(&scheduler_state->priority_medium, proc, priority_pointers.prev, priority_pointers.next);
                } else if (priority == PRIORITY_LOW) {
                    if (proc->priority == PRIORITY_HIGH) {
                        linked_list_remove(&scheduler_state->priority_high, proc, priority_pointers.prev, priority_pointers.next);
                    } else if (proc->priority == PRIORITY_MEDIUM) {
                        linked_list_remove(&scheduler_state->priority_medium, proc, priority_pointers.prev, priority_pointers.next);
                    }
                    proc->priority = priority;
                    linked_list_push_tail(&scheduler_state->priority_low, proc, priority_pointers.prev, priority_pointers.next);
                }
            }
            return 0;
        }
        proc = proc->process_pointers.next;
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
    
