#include <stdio.h>
#include <stdlib.h> 
#include "scheduler.h"
#include "logger.h"
#include "../../lib/exiting_alloc.h"
#include "../../lib/linked_list.h"
#include "spthread.h"
#include <sys/time.h>
#include "kernel.h"


scheduler_t* scheduler_state = NULL;
static const int centisecond = 10000;
static int quantum = 0;
static sigset_t suspend_set;

static void alarm_handler(int signum) {}


// Helper functions for linked list operations
static void linked_list_remove_with_pointers(linked_list(pcb_t)* list, pcb_t* proc, 
                                           struct process_control_block** prev_ptr, 
                                           struct process_control_block** next_ptr) {
    // Remove from the list
    linked_list_remove(list, proc);
    
    // Update the pointers in the PCB
    *prev_ptr = NULL;
    *next_ptr = NULL;
}

static void linked_list_push_tail_with_pointers(linked_list(pcb_t)* list, pcb_t* proc,
                                              struct process_control_block** prev_ptr,
                                              struct process_control_block** next_ptr) {
    // Push to the list
    linked_list_push_tail(list, proc);
    
    // Update the pointers in the PCB
    if (list->head == proc) {
        *prev_ptr = NULL;
    } else {
        pcb_t* prev = list->head;
        while (prev->next != proc) {
            prev = prev->next;
        }
        *prev_ptr = prev;
    }
    
    if (list->tail == proc) {
        *next_ptr = NULL;
    } else {
        pcb_t* next = list->tail;
        while (next->prev != proc) {
            next = next->prev;
        }
        *next_ptr = next;
    }
}

// Init thread main function - continuously reaps zombie children
static void* init_thread_func(void* arg) {
    LOG_INFO("running init thread");
    
    while (1) {
        
        if (scheduler_state->terminated_processes.head != NULL) {
            LOG_INFO("INITIALIZE");
            log_process_state();
            // Get the process to terminate
            pcb_t* terminated = scheduler_state->terminated_processes.head;
            
            // Only clean up orphaned processes (those whose parent has terminated)
            // or processes whose parent is init (pid 1)
            if (terminated->ppid <= 1) {
                // Remove from terminated queue first
                linked_list_remove_with_pointers(&scheduler_state->terminated_processes, terminated, 
                                               &terminated->priority_prev, &terminated->priority_next);
                // Remove from main process list before cleanup
                log_process_state();
                
                k_proc_cleanup(terminated);
            } else {
                LOG_INFO("Moving process %d to end of terminated queue", terminated->pid);
                // Move to end of terminated queue if parent still exists
                linked_list_remove_with_pointers(&scheduler_state->terminated_processes, terminated, 
                                               &terminated->prev, &terminated->next);
                linked_list_push_tail_with_pointers(&scheduler_state->terminated_processes, terminated, 
                                                  &terminated->prev, &terminated->next);
            }
            log_process_state();
        }
    }
    return NULL;
}

void init_scheduler() {
    scheduler_state = (scheduler_t*) exiting_malloc(sizeof(scheduler_t));
    
    init_logger("scheduler.log");
    scheduler_state->process_count = 0;
    
    // Initialize all linked lists
    scheduler_state->processes = linked_list_new(pcb_t, NULL);
    scheduler_state->priority_high = linked_list_new(pcb_t, NULL);
    scheduler_state->priority_medium = linked_list_new(pcb_t, NULL);
    scheduler_state->priority_low = linked_list_new(pcb_t, NULL);
    scheduler_state->blocked_processes = linked_list_new(pcb_t, NULL);
    scheduler_state->terminated_processes = linked_list_new(pcb_t, NULL);
    scheduler_state->sleeping_processes = linked_list_new(pcb_t, NULL);
    
    pcb_t* init = exiting_malloc(sizeof(pcb_t));
    init->prev = NULL;
    init->next = NULL;
    init->priority_prev = NULL;
    init->priority_next = NULL;
    init->child_prev = NULL;
    init->child_next = NULL;
    init->ppid = 0;
    init->pid = scheduler_state->process_count++;
    init->pgid = init->pid;
    init->fd0 = -1;
    init->fd1 = -1;
    init->is_leader = true;
    init->sleep_time = 0;
    init->priority = PRIORITY_HIGH;
    init->state = PROCESS_RUNNING;
    init->command = "init";
    init->children = linked_list_new(child_process_t, NULL);
    init->argv = NULL;
    
    init->thread = (spthread_t*)exiting_malloc(sizeof(spthread_t));
    if (spthread_create(init->thread, NULL, init_thread_func, NULL) != 0) {
        LOG_ERROR("Failed to create init thread");
        exit(1);
    }
    
    scheduler_state->curr = init;
    scheduler_state->init = init;
    linked_list_push_tail_with_pointers(&scheduler_state->priority_high, init, 
                                     &init->priority_prev, &init->priority_next);
    linked_list_push_tail_with_pointers(&scheduler_state->processes, init, 
                                     &init->prev, &init->next);
    sigfillset(&suspend_set);
    sigdelset(&suspend_set, SIGALRM);
    struct sigaction act = (struct sigaction){
        .sa_handler = alarm_handler,
        .sa_mask = suspend_set,
        .sa_flags = SA_RESTART,
    };
    sigaction(SIGALRM, &act, NULL);
    sigset_t alarm_set;
    sigemptyset(&alarm_set);
    sigaddset(&alarm_set, SIGALRM);
    pthread_sigmask(SIG_UNBLOCK, &alarm_set, NULL);
    
    struct itimerval it;
    it.it_interval = (struct timeval){.tv_usec = centisecond * 10};
    it.it_value = it.it_interval;
    setitimer(ITIMER_REAL, &it, NULL);
    
    quantum = 0;  // Reset quantum counter
    update_ticks(quantum);  // Initialize logger ticks to 0
}

void decrease_sleep() {
    if (scheduler_state->sleeping_processes.head == NULL) {
        return;
    }
    
    pcb_t* proc = scheduler_state->sleeping_processes.head;
    while (proc != NULL) {
        pcb_t* next = proc->next;
        
        proc->sleep_time -= 1;
        if (proc->sleep_time <= 0) {
            // Remove from sleeping queue
            linked_list_remove_with_pointers(&scheduler_state->sleeping_processes, proc, 
                                          &proc->prev, &proc->next);
            
            // Add back to appropriate priority queue
            add_process_to_queue(proc);
            
            log_wake(proc->pid, proc->priority, proc->command);
        }
        
        proc = next;
    }
}

void log_process_state() {
    LOG_INFO("=== Current Process State ===");
    
    LOG_INFO("Current process: %d (State: %d)", scheduler_state->curr->pid, scheduler_state->curr->state);
    LOG_INFO("Parent process: %d (State: %d)", scheduler_state->curr->ppid, scheduler_state->curr->state);
    LOG_INFO("=== Current Process State ===");
    pcb_t* proc = scheduler_state->processes.head;
    while (proc != NULL) {
        LOG_INFO("  PID %d (State: %d)", proc->pid, proc->state);
        proc = proc->process_pointers.next;
    }
    
}

// Log all processes in each queue
void log_queue_state() {
    LOG_INFO("=== Current Queue State ===");
    
    LOG_INFO("High Priority Queue:");
    pcb_t* high = scheduler_state->priority_high.head;
    while (high != NULL) {
        LOG_INFO("  PID %d (State: %d)", high->pid, high->state);
        high = high->priority_pointers.next;
    }
    
    LOG_INFO("Medium Priority Queue:");
    pcb_t* med = scheduler_state->priority_medium.head;
    while (med != NULL) {
        LOG_INFO("  PID %d (State: %d)", med->pid, med->state);
        med = med->priority_pointers.next;
    }
    
    LOG_INFO("Low Priority Queue:");
    pcb_t* low = scheduler_state->priority_low.head;
    while (low != NULL) {
        LOG_INFO("  PID %d (State: %d)", low->pid, low->state);
        low = low->priority_pointers.next;
    }
    
    LOG_INFO("Sleeping Queue:");
    pcb_t* sleep = scheduler_state->sleeping_processes.head;
    while (sleep != NULL) {
        LOG_INFO("  PID %d (Sleep time: %f)", sleep->pid, sleep->sleep_time);
        sleep = sleep->priority_pointers.next;
    }
    
    LOG_INFO("Terminated Queue:");
    pcb_t* term = scheduler_state->terminated_processes.head;
    while (term != NULL) {
        LOG_INFO("  PID %d", term->pid);
        term = term->priority_pointers.next;
    }
    
    LOG_INFO("========================");
}

// Check if there are any runnable processes
static bool has_runnable_processes() {
    // Check high priority queue
    if (scheduler_state->priority_high.head != NULL) {
        if (((pcb_t*)scheduler_state->priority_high.head)->pid != 0) {
            return true;
        }
    }

    // Check medium priority queue, but if it only contains init process (pid 0), don't count it
    if (scheduler_state->priority_medium.head != NULL) {
        return true;
        
    }
    if (scheduler_state->priority_low.head != NULL)
        return true;

    return false;
}

void run_scheduler() {
    sigset_t mask;
    sigfillset(&mask);
    sigdelset(&mask, SIGALRM);
    
    while (1) {
        
        // Only run processes and decrease sleep if we have runnable processes
        run_next_process();
        decrease_sleep();
        
        // Always suspend to wait for next signal
        sigsuspend(&mask);
    }
}

bool should_block_process_waitpid(pcb_t* proc) {
    if (proc->pid <= 1) {  // Shell or Init
        // Check if process is waiting for children
        pcb_t* child = proc->children.head;
        while (child != NULL && child->pid != 1) {
            LOG_INFO("Checking child %d", child->pid);
            if (child->state != PROCESS_ZOMBIED) {
                return true;  // Block if any non-terminated children exist
            }
            child = child->process_pointers.next;
        }
    }
    return false;
}

void block_process(pcb_t* proc) {
    proc->state = PROCESS_BLOCKED;
    linked_list_push_tail_with_pointers(&scheduler_state->blocked_processes, proc, 
                                      &proc->priority_prev, &proc->priority_next);
    LOG_INFO("Process %d blocked waiting for child", proc->pid);
    log_blocked(proc->pid, proc->priority, proc->command);
}

void add_process_to_queue(pcb_t *proc) {
    LOG_INFO("Adding PID %d (priority %d) at address %p", proc->pid, proc->priority, proc);
    if (proc->priority == PRIORITY_HIGH) {
        LOG_INFO("  Target queue: HIGH (list addr: %p)", &scheduler_state->priority_high);
        linked_list_push_tail_with_pointers(&scheduler_state->priority_high, proc, 
                                          &proc->priority_prev, &proc->priority_next);
    } else if (proc->priority == PRIORITY_MEDIUM) {
        LOG_INFO("  Target queue: MEDIUM (list addr: %p)", &scheduler_state->priority_medium);
        linked_list_push_tail_with_pointers(&scheduler_state->priority_medium, proc, 
                                          &proc->priority_prev, &proc->priority_next);
    } else {
        LOG_INFO("  Target queue: LOW (list addr: %p)", &scheduler_state->priority_low);
        linked_list_push_tail_with_pointers(&scheduler_state->priority_low, proc, 
                                          &proc->priority_prev, &proc->priority_next);
    }
    LOG_INFO("  After add - proc prev: %p, next: %p", proc->priority_prev, proc->priority_next);
}

void run_next_process() {
    int current = quantum % 18;
    LOG_INFO("CURRENT: %d", current);

    // Try high priority first if in high priority time slot
    if (current < 9) {
        if (scheduler_state->priority_high.head != NULL) {
            LOG_INFO("Running high priority process");
            pcb_t* proc = scheduler_state->priority_high.head;
            if(proc->state == PROCESS_BLOCKED) {
                linked_list_remove_with_pointers(&scheduler_state->priority_high, proc, 
                                              &proc->priority_prev, &proc->priority_next);
                linked_list_push_tail_with_pointers(&scheduler_state->priority_high, proc, 
                                                 &proc->priority_prev, &proc->priority_next);
                quantum++;
                return;
            }
            
            // Check if shell/init should be blocked for waitpid
            // if (proc->pid <= 1 && should_block_process_waitpid(proc)) {
            //     block_process(proc);
            //     quantum++;
            //     return;
            // }
            
            scheduler_state->curr = proc;
            log_schedule(proc->pid, PRIORITY_HIGH, proc->command);
            spthread_continue(*proc->thread);
            sigsuspend(&suspend_set);
            int ret = spthread_suspend(*proc->thread);
            linked_list_remove_with_pointers(&scheduler_state->priority_high, proc, 
                                          &proc->priority_prev, &proc->priority_next);
            if (ret != 0 && proc->pid > 1) {
                // Log before state change
                LOG_INFO("SCHEDULER: Set PID %d state to ZOMBIED (HIGH PRIO)", proc->pid);
                proc->state = PROCESS_ZOMBIED;
                LOG_INFO("Process %d terminated", proc->pid);
                pcb_t* child = proc->children.head;
                while (child != NULL) {
                    child->ppid = 0;
                    linked_list_push_tail_with_pointers(&scheduler_state->init->children, child, 
                                                     &child->child_prev, &child->child_next);
                    child = child->next;
                }
                log_exited(proc->pid, proc->priority, proc->command);
            } else {
                // Re-add non-terminated processes and special PIDs (0 and 1)
                linked_list_push_tail_with_pointers(&scheduler_state->priority_high, proc, 
                                                 &proc->priority_prev, &proc->priority_next);
            }
            quantum++;
            update_ticks(quantum);  // Update ticks in logger
            return;
        }
    }
    if (current < 15 || (current < 9 && scheduler_state->priority_high.head == NULL)) {
        LOG_INFO("Running medium priority process");
        if (scheduler_state->priority_medium.head != NULL) {
            pcb_t* proc = scheduler_state->priority_medium.head;
            if(proc->state == PROCESS_BLOCKED) {
                linked_list_remove_with_pointers(&scheduler_state->priority_medium, proc, 
                                              &proc->priority_prev, &proc->priority_next);
                linked_list_push_tail_with_pointers(&scheduler_state->priority_medium, proc, 
                                                 &proc->priority_prev, &proc->priority_next);
                quantum++;
                return;
            }
            
            scheduler_state->curr = proc;
            log_schedule(proc->pid, PRIORITY_MEDIUM, proc->command);
            spthread_continue(*proc->thread);
            sigsuspend(&suspend_set);
            int ret = spthread_suspend(*proc->thread);
            linked_list_remove_with_pointers(&scheduler_state->priority_medium, proc, 
                                          &proc->priority_prev, &proc->priority_next);
            if (ret != 0 && proc->pid > 1) {
                // Log before state change
                LOG_INFO("SCHEDULER: Set PID %d state to ZOMBIED (MEDIUM PRIO)", proc->pid);
                proc->state = PROCESS_ZOMBIED;
                pcb_t* child = proc->children.head;
                while (child != NULL) {
                    child->ppid = 0;
                    linked_list_push_tail_with_pointers(&scheduler_state->init->children, child, 
                                                     &child->child_prev, &child->child_next);
                    child = child->next;
                }
                LOG_INFO("Process %d terminated", proc->pid);
                log_exited(proc->pid, proc->priority, proc->command);
            } else {
                // Re-add non-terminated processes and special PIDs (0 and 1)
                linked_list_push_tail_with_pointers(&scheduler_state->priority_medium, proc, 
                                                 &proc->priority_prev, &proc->priority_next);
            }
            quantum++;
            update_ticks(quantum);  // Update ticks in logger
            return;
        }
    }
    if (current >= 15 || 
        ((current < 15 && scheduler_state->priority_medium.head == NULL) && 
         (current < 9 && scheduler_state->priority_high.head == NULL))) {
        if (scheduler_state->priority_low.head != NULL) {
            pcb_t* proc = scheduler_state->priority_low.head;
            if(proc->state == PROCESS_BLOCKED) {
                linked_list_remove_with_pointers(&scheduler_state->priority_low, proc, 
                                              &proc->priority_prev, &proc->priority_next);
                linked_list_push_tail_with_pointers(&scheduler_state->priority_low, proc, 
                                                 &proc->priority_prev, &proc->priority_next);
                quantum++;
                return;
            }
            
            scheduler_state->curr = proc;
            log_schedule(proc->pid, PRIORITY_LOW, proc->command);
            spthread_continue(*proc->thread);
            sigsuspend(&suspend_set);
            int ret = spthread_suspend(*proc->thread);
            linked_list_remove_with_pointers(&scheduler_state->priority_low, proc, 
                                          &proc->priority_prev, &proc->priority_next);
            if (ret != 0 && proc->pid > 1) {
                // Log before state change
                LOG_INFO("SCHEDULER: Set PID %d state to ZOMBIED (LOW PRIO)", proc->pid);
                proc->state = PROCESS_ZOMBIED;
                LOG_INFO("Process %d terminated", proc->pid);
                pcb_t* child = proc->children.head;
                while (child != NULL) {
                    child->ppid = 0;
                    child = child->next;
                }
                log_exited(proc->pid, proc->priority, proc->command);
            } else {
                // Re-add non-terminated processes and special PIDs (0 and 1)
                linked_list_push_tail_with_pointers(&scheduler_state->priority_low, proc, 
                                                 &proc->priority_prev, &proc->priority_next);
            }
            quantum++;
            update_ticks(quantum);  // Update ticks in logger
            return;
        }
        // If we reach here, no processes in any priority queue are ready
        if (!has_runnable_processes()) {
            LOG_INFO("No runnable processes, scheduler idling");
            // Use sigsuspend to idle until a signal arrives
            quantum += 3;
            update_ticks(quantum);  // Update ticks in logger
            sigsuspend(&suspend_set);
            // After waking up, don't increment quantum - let the next iteration handle that
            return;
        }
    }
    quantum++;
    update_ticks(quantum);  // Update ticks in logger
}

void log_all_processes() {
    
}

// Function to unblock a previously blocked process
void unblock_process(pcb_t* proc) {
    LOG_INFO("Unblocking process %d", proc->pid);
    proc->state = PROCESS_RUNNING;
    
    // Remove from blocked queue
    linked_list_remove_with_pointers(&scheduler_state->blocked_processes, proc, 
                                  &proc->priority_prev, &proc->priority_next);
    
    // Add to appropriate priority queue
    add_process_to_queue(proc);
    
    log_unblocked(proc->pid, proc->priority, proc->command);
}

// Function to stop a process (similar to SIGSTOP in Unix)
void stop_process(pcb_t* proc) {
    LOG_INFO("Stopping process %d", proc->pid);
    proc->state = PROCESS_STOPPED;
    
    // Remove from all priority queues
    linked_list_remove_with_pointers(&scheduler_state->priority_high, proc, 
                                  &proc->priority_prev, &proc->priority_next);
    linked_list_remove_with_pointers(&scheduler_state->priority_medium, proc, 
                                  &proc->priority_prev, &proc->priority_next);
    linked_list_remove_with_pointers(&scheduler_state->priority_low, proc, 
                                  &proc->priority_prev, &proc->priority_next);
    
    // Add to blocked queue
    linked_list_push_tail_with_pointers(&scheduler_state->blocked_processes, proc, 
                                     &proc->priority_prev, &proc->priority_next);
    
    log_stopped(proc->pid, proc->priority, proc->command);
}

// Function to continue a stopped process (similar to SIGCONT in Unix)
void continue_process(pcb_t* proc) {
    LOG_INFO("Continuing process %d", proc->pid);
    proc->state = PROCESS_RUNNING;
    
    // Remove from blocked queue
    linked_list_remove_with_pointers(&scheduler_state->blocked_processes, proc, 
                                  &proc->priority_prev, &proc->priority_next);
    
    // Add to appropriate priority queue
    add_process_to_queue(proc);
    
    log_continued(proc->pid, proc->priority, proc->command);
}

void put_process_to_sleep(pcb_t* proc, unsigned int ticks) {
    LOG_INFO("Putting process %d to sleep for %u ticks", proc->pid, ticks);
    
    // Remove from current queue
    if (proc->priority == PRIORITY_HIGH) {
        linked_list_remove_with_pointers(&scheduler_state->priority_high, proc, 
                                      &proc->priority_prev, &proc->priority_next);
    } else if (proc->priority == PRIORITY_MEDIUM) {
        linked_list_remove_with_pointers(&scheduler_state->priority_medium, proc, 
                                      &proc->priority_prev, &proc->priority_next);
    } else {
        linked_list_remove_with_pointers(&scheduler_state->priority_low, proc, 
                                      &proc->priority_prev, &proc->priority_next);
    }
    
    // Set sleep time
    proc->sleep_time = ticks;
    
    // Add to sleeping queue
    linked_list_push_tail_with_pointers(&scheduler_state->sleeping_processes, proc, 
                                     &proc->prev, &proc->next);
    
    log_sleep(proc->pid, proc->priority, proc->command, ticks);
}

void terminate_process(pcb_t* process) {
    LOG_INFO("Terminating process %d", process->pid);
    
    // Remove from all queues
    linked_list_remove_with_pointers(&scheduler_state->priority_high, process, 
                                  &process->priority_prev, &process->priority_next);
    linked_list_remove_with_pointers(&scheduler_state->priority_medium, process, 
                                  &process->priority_prev, &process->priority_next);
    linked_list_remove_with_pointers(&scheduler_state->priority_low, process, 
                                  &process->priority_prev, &process->priority_next);
    linked_list_remove_with_pointers(&scheduler_state->blocked_processes, process, 
                                  &process->priority_prev, &process->priority_next);
    linked_list_remove_with_pointers(&scheduler_state->sleeping_processes, process, 
                                  &process->prev, &process->next);
    
    // Set state to zombied
    process->state = PROCESS_ZOMBIED;
    
    // Add to terminated queue
    linked_list_push_tail_with_pointers(&scheduler_state->terminated_processes, process, 
                                     &process->priority_prev, &process->priority_next);
    
    log_terminated(process->pid, process->priority, process->command);
}

void handle_orphaned_processes(pcb_t* terminated_process) {
    LOG_INFO("Handling orphaned processes for terminated process %d", terminated_process->pid);
    
    // Get all children of the terminated process
    pcb_t* child = terminated_process->children.head;
    while (child != NULL) {
        pcb_t* next = child->next;
        
        // Set parent to init (PID 1)
        child->ppid = 1;
        
        // Remove from parent's children list
        linked_list_remove_with_pointers(&terminated_process->children, child, 
                                      &child->child_prev, &child->child_next);
        
        // Add to init's children list
        linked_list_push_tail_with_pointers(&scheduler_state->init->children, child, 
                                         &child->child_prev, &child->child_next);
        
        LOG_INFO("Process %d is now orphaned, parent set to init", child->pid);
        
        child = next;
    }
}

void cleanup_zombie_children(pcb_t* parent) {
    LOG_INFO("Cleaning up zombie children for process %d", parent->pid);
    
    // Get all children of the parent
    pcb_t* child = parent->children.head;
    while (child != NULL) {
        pcb_t* next = child->next;
        
        // Check if child is a zombie
        if (child->state == PROCESS_ZOMBIED) {
            // Remove from parent's children list
            linked_list_remove_with_pointers(&parent->children, child, 
                                          &child->child_prev, &child->child_next);
            
            // Free the child's resources
            free(child->command);
            free(child->argv);
            free(child);
            
            LOG_INFO("Cleaned up zombie child of process %d", parent->pid);
        }
        
        child = next;
    }
}
