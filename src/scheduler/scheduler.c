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


// Init thread main function - continuously reaps zombie children
static void* init_thread_func(void* arg) {
    LOG_INFO("running init thread");
    while (1) {
        if (scheduler_state->terminated_processes.head != NULL) {
            // Get the process to terminate
            pcb_t* terminated = scheduler_state->terminated_processes.head;
            LOG_INFO("Processing terminated process %d", terminated->pid);
            
            // Remove from terminated queue first
            scheduler_state->terminated_processes.head = terminated->process_pointers.next;
            if (terminated->process_pointers.next != NULL) {
                terminated->process_pointers.next->process_pointers.prev = NULL;
            } else {
                scheduler_state->terminated_processes.tail = NULL;
            }
            terminated->process_pointers.next = NULL;
            terminated->process_pointers.prev = NULL;
            
            // The process should already be removed from its priority queue in run_next_process
            // when it was detected as terminated. We just need to clean it up.
            LOG_INFO("Init cleaning up terminated process %d", terminated->pid);
            k_proc_cleanup(terminated);        }
    }
   return NULL;
}

void init_scheduler() {
    scheduler_state = (scheduler_t*) exiting_malloc(sizeof(scheduler_t));
    
    init_logger("scheduler.log");
    scheduler_state->process_count = 0;
    
    // Initialize all linked lists
    scheduler_state->processes.head = NULL;
    scheduler_state->processes.tail = NULL;
    scheduler_state->processes.ele_dtor = NULL;
    
    scheduler_state->priority_high.head = NULL;
    scheduler_state->priority_high.tail = NULL;
    scheduler_state->priority_high.ele_dtor = NULL;
    
    scheduler_state->priority_medium.head = NULL;
    scheduler_state->priority_medium.tail = NULL;
    scheduler_state->priority_medium.ele_dtor = NULL;
    
    scheduler_state->priority_low.head = NULL;
    scheduler_state->priority_low.tail = NULL;
    scheduler_state->priority_low.ele_dtor = NULL;
    
    scheduler_state->blocked_processes.head = NULL;
    scheduler_state->blocked_processes.tail = NULL;
    scheduler_state->blocked_processes.ele_dtor = NULL;
    
    scheduler_state->terminated_processes.head = NULL;
    scheduler_state->terminated_processes.tail = NULL;
    scheduler_state->terminated_processes.ele_dtor = NULL;

    scheduler_state->sleeping_processes.head = NULL;
    scheduler_state->sleeping_processes.tail = NULL;
    scheduler_state->sleeping_processes.ele_dtor = NULL;
    
    
    pcb_t* init = exiting_malloc(sizeof(pcb_t));
    init->priority_pointers.prev = NULL;
    init->priority_pointers.next = NULL;
    init->process_pointers.prev = NULL;
    init->process_pointers.next = NULL;
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
    init->child_pointers.prev = NULL;
    init->child_pointers.next = NULL;
    
    init->children.head = NULL;
    init->children.tail = NULL;
    init->children.ele_dtor = NULL;
    init->argv = NULL;
    

    init->thread = (spthread_t*)exiting_malloc(sizeof(spthread_t));
    if (spthread_create(init->thread, NULL, init_thread_func, NULL) != 0) {
        LOG_ERROR("Failed to create init thread");
        exit(1);
    }
    

    scheduler_state->curr = init;
    scheduler_state->init = init; 
    linked_list_push_tail(&scheduler_state->priority_high, init, priority_pointers.prev, priority_pointers.next);
    linked_list_push_tail(&scheduler_state->processes, init, process_pointers.prev, process_pointers.next);
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
    // run_scheduler();

}

void decrease_sleep() {
    pcb_t* proc = scheduler_state->sleeping_processes.head;
    while (proc != NULL) {
        proc->sleep_time -= centisecond;
        if (proc->sleep_time <= 0) {
            LOG_INFO("Process %d is no longer sleeping", proc->pid);
            proc->state = PROCESS_READY;
            linked_list_remove(&scheduler_state->sleeping_processes, proc, process_pointers.prev, process_pointers.next);
            linked_list_push_tail(&scheduler_state->processes, proc, process_pointers.prev, process_pointers.next);
        }
        proc = proc->priority_pointers.next;
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
        log_queue_state();
        
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
            if (child->state != PROCESS_TERMINATED) {
                return true;  // Block if any non-terminated children exist
            }
            child = child->process_pointers.next;
        }
    }
    return false;
}

void block_process(pcb_t* proc) {
    if (proc->priority == PRIORITY_HIGH) {
        linked_list_remove(&scheduler_state->priority_high, proc, priority_pointers.prev, priority_pointers.next);
    } else if (proc->priority == PRIORITY_MEDIUM) {
        linked_list_remove(&scheduler_state->priority_medium, proc, priority_pointers.prev, priority_pointers.next);
    } else {
        linked_list_remove(&scheduler_state->priority_low, proc, priority_pointers.prev, priority_pointers.next);
    }
    
    proc->state = PROCESS_BLOCKED;
    linked_list_push_tail(&scheduler_state->blocked_processes, proc, priority_pointers.prev, priority_pointers.next);
    LOG_INFO("Process %d blocked waiting for child", proc->pid);
}

void add_process_to_queue(pcb_t *proc) {
    LOG_INFO("Adding PID %d (priority %d) at address %p", proc->pid, proc->priority, proc);
    if (proc->priority == PRIORITY_HIGH) {
        LOG_INFO("  Target queue: HIGH (list addr: %p)", &scheduler_state->priority_high);
        linked_list_push_tail(&scheduler_state->priority_high, proc, priority_pointers.prev, priority_pointers.next);
    } else if (proc->priority == PRIORITY_MEDIUM) {
        LOG_INFO("  Target queue: MEDIUM (list addr: %p)", &scheduler_state->priority_medium);
        linked_list_push_tail(&scheduler_state->priority_medium, proc, priority_pointers.prev, priority_pointers.next);
    } else {
        LOG_INFO("  Target queue: LOW (list addr: %p)", &scheduler_state->priority_low);
        linked_list_push_tail(&scheduler_state->priority_low, proc, priority_pointers.prev, priority_pointers.next);
    }
    LOG_INFO("  After add - proc prev: %p, next: %p", proc->priority_pointers.prev, proc->priority_pointers.next);
}

void run_next_process() {
    int current = quantum % 18;
    LOG_INFO("CURRENT: %d", current);

    // Try high priority first if in high priority time slot
    if (current < 9) {
        if (scheduler_state->priority_high.head != NULL) {
            LOG_INFO("Running high priority process");
            pcb_t* proc = scheduler_state->priority_high.head;
            
            // Check if shell/init should be blocked for waitpid
            // if (proc->pid <= 1 && should_block_process_waitpid(proc)) {
            //     block_process(proc);
            //     quantum++;
            //     return;
            // }
            
            scheduler_state->curr = proc;
            spthread_continue(*proc->thread);
            sigsuspend(&suspend_set);
            int ret = spthread_suspend(*proc->thread);
            linked_list_remove(&scheduler_state->priority_high, proc, priority_pointers.prev, priority_pointers.next);
            if (ret != 0 && proc->pid > 1) {
                // Reset prev/next pointers before adding to terminated queue
                proc->priority_pointers.prev = NULL;
                proc->priority_pointers.next = NULL;
                linked_list_push_tail(&scheduler_state->terminated_processes, proc, priority_pointers.prev, priority_pointers.next);
                LOG_INFO("Process %d terminated", proc->pid);
            } else {
                // Re-add non-terminated processes and special PIDs (0 and 1)
                linked_list_push_tail(&scheduler_state->priority_high, proc, priority_pointers.prev, priority_pointers.next);
            }
            quantum++;
            return;
        }
    }
    if (current < 15 || (current < 9 && scheduler_state->priority_high.head == NULL)) {
        LOG_INFO("Running medium priority process");
        if (scheduler_state->priority_medium.head != NULL) {
            pcb_t* proc = scheduler_state->priority_medium.head;
            
            scheduler_state->curr = proc;
            spthread_continue(*proc->thread);
            sigsuspend(&suspend_set);
            int ret = spthread_suspend(*proc->thread);
            linked_list_remove(&scheduler_state->priority_medium, proc, priority_pointers.prev, priority_pointers.next);
            if (ret != 0 && proc->pid > 1) {
                // Reset prev/next pointers before adding to terminated queue
                proc->priority_pointers.prev = NULL;
                proc->priority_pointers.next = NULL;
                linked_list_push_tail(&scheduler_state->terminated_processes, proc, priority_pointers.prev, priority_pointers.next);
                LOG_INFO("Process %d terminated", proc->pid);
            } else {
                // Re-add non-terminated processes and special PIDs (0 and 1)
                linked_list_push_tail(&scheduler_state->priority_medium, proc, priority_pointers.prev, priority_pointers.next);
            }
            quantum++;
            return;
        }
    }
    if (current >= 15 || 
        ((current < 15 && scheduler_state->priority_medium.head == NULL) && 
         (current < 9 && scheduler_state->priority_high.head == NULL))) {
        if (scheduler_state->priority_low.head != NULL) {
            pcb_t* proc = scheduler_state->priority_low.head;
            
            scheduler_state->curr = proc;
            spthread_continue(*proc->thread);
            sigsuspend(&suspend_set);
            int ret = spthread_suspend(*proc->thread);
            linked_list_remove(&scheduler_state->priority_low, proc, priority_pointers.prev, priority_pointers.next);
            if (ret != 0 && proc->pid > 1) {
                // Reset prev/next pointers before adding to terminated queue
                proc->priority_pointers.prev = NULL;
                proc->priority_pointers.next = NULL;
                linked_list_push_tail(&scheduler_state->terminated_processes, proc, priority_pointers.prev, priority_pointers.next);
                LOG_INFO("Process %d terminated", proc->pid);
            } else {
                // Re-add non-terminated processes and special PIDs (0 and 1)
                linked_list_push_tail(&scheduler_state->priority_low, proc, priority_pointers.prev, priority_pointers.next);
            }
            quantum++;
            return;
        }
        // If we reach here, no processes in any priority queue are ready
        if (!has_runnable_processes()) {
            LOG_INFO("No runnable processes, scheduler idling");
            // Use sigsuspend to idle until a signal arrives
            quantum += 3;
            sigsuspend(&suspend_set);
            // After waking up, don't increment quantum - let the next iteration handle that
            return;
        }
    }
    quantum++;
}

void log_all_processes() {
    
}
