#include <stdio.h>
#include <stdlib.h> 
#include "scheduler.h"
#include "logger.h"
#include "../shell/exiting_alloc.h"
#include "../lib/linked_list.h"
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
    while (1) {
        LOG_INFO("Init thread running");
        if (scheduler_state->terminated_processes.head != NULL) {
            pcb_t* terminated = linked_list_pop_head(&scheduler_state->terminated_processes);
            LOG_INFO("Init cleaning up terminated process %d", terminated->pid);
            free(terminated);
        }
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
    init->ppid = 0;
    init->pid = scheduler_state->process_count++;
    init->pgid = init->pid;
    init->fd0 = -1;
    init->fd1 = -1;
    init->is_leader = true;
    init->sleep_time = 0;
    init->priority = PRIORITY_MEDIUM;
    init->state = PROCESS_RUNNING;
    init->prev = NULL;
    init->next = NULL;
    

    init->thread = (spthread_t*)exiting_malloc(sizeof(spthread_t));
    if (spthread_create(init->thread, NULL, init_thread_func, NULL) != 0) {
        LOG_ERROR("Failed to create init thread");
        exit(1);
    }
    

    scheduler_state->curr = init;
    scheduler_state->init = init; 
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
            linked_list_remove(&scheduler_state->sleeping_processes, proc);
            linked_list_push_tail(&scheduler_state->processes, proc);
        }
        proc = proc->next;
    }
}


// Log all processes in each queue
static void log_queue_state() {
    LOG_INFO("=== Current Queue State ===");
    
    LOG_INFO("High Priority Queue:");
    pcb_t* high = scheduler_state->priority_high.head;
    while (high != NULL) {
        LOG_INFO("  PID %d (State: %d)", high->pid, high->state);
        high = high->next;
    }
    
    LOG_INFO("Medium Priority Queue:");
    pcb_t* med = scheduler_state->priority_medium.head;
    while (med != NULL) {
        LOG_INFO("  PID %d (State: %d)", med->pid, med->state);
        med = med->next;
    }
    
    LOG_INFO("Low Priority Queue:");
    pcb_t* low = scheduler_state->priority_low.head;
    while (low != NULL) {
        LOG_INFO("  PID %d (State: %d)", low->pid, low->state);
        low = low->next;
    }
    
    LOG_INFO("Sleeping Queue:");
    pcb_t* sleep = scheduler_state->sleeping_processes.head;
    while (sleep != NULL) {
        LOG_INFO("  PID %d (Sleep time: %f)", sleep->pid, sleep->sleep_time);
        sleep = sleep->next;
    }
    
    LOG_INFO("Terminated Queue:");
    pcb_t* term = scheduler_state->terminated_processes.head;
    while (term != NULL) {
        LOG_INFO("  PID %d", term->pid);
        term = term->next;
    }
    
    LOG_INFO("========================");
}

// Check if there are any runnable processes
static bool has_runnable_processes() {
    return scheduler_state->priority_high.head != NULL ||
           scheduler_state->priority_medium.head != NULL ||
           scheduler_state->priority_low.head != NULL ||
           scheduler_state->sleeping_processes.head != NULL;
}

void run_scheduler() {
    sigset_t mask;
    sigfillset(&mask);
    sigdelset(&mask, SIGALRM);
    
    while (has_runnable_processes()) {
        log_queue_state();
        sigsuspend(&mask); 
        run_next_process();
        decrease_sleep();
    }
    
    LOG_INFO("No more runnable processes, scheduler exiting");
}

void block_process(pcb_t* proc) {

    if (proc->priority == PRIORITY_HIGH) {
        linked_list_remove(&scheduler_state->priority_high, proc);
    } else if (proc->priority == PRIORITY_MEDIUM) {
        linked_list_remove(&scheduler_state->priority_medium, proc);
    } else {
        linked_list_remove(&scheduler_state->priority_low, proc);
    }
    
    proc->state = PROCESS_BLOCKED;
    linked_list_push_tail(&scheduler_state->blocked_processes, proc);
    LOG_INFO("Process %d blocked after %d quantums", proc->pid, quantum);
}

void add_process_to_queue(pcb_t* proc) {
    if (proc->priority == PRIORITY_HIGH) {
        linked_list_push_tail(&scheduler_state->priority_high, proc);
    } else if (proc->priority == PRIORITY_MEDIUM) {
        linked_list_push_tail(&scheduler_state->priority_medium, proc);
    } else {
        linked_list_push_tail(&scheduler_state->priority_low, proc);
    }
}

void run_next_process() {
    int current = quantum % 18;
    LOG_INFO("Quantum %d", quantum);

    
    // Always run init process if it exists and there are no other processes
    if (!has_runnable_processes()) {
        if (scheduler_state->init != NULL) {
            scheduler_state->curr = scheduler_state->init;
            spthread_continue(*scheduler_state->init->thread);
            sigsuspend(&suspend_set);
            spthread_suspend(*scheduler_state->init->thread);
            current++;
            return;
        }
        current++;
        return;
    }
    LOG_INFO("No init process, running next process");
    // Try high priority first if in high priority time slot
    if (current < 9) {
        if (scheduler_state->priority_high.head != NULL) {
            LOG_INFO("Running high priority process");
            pcb_t* proc = scheduler_state->priority_high.head;
            
            scheduler_state->curr = proc;
            spthread_continue(*proc->thread);
            sigsuspend(&suspend_set);
            spthread_suspend(*proc->thread);
            
            // Move to back of queue for next round if suspended
            // After suspending, move to back of queue
            linked_list_remove(&scheduler_state->priority_high, proc);
            linked_list_push_tail(&scheduler_state->priority_high, proc);
            current++;
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
            spthread_suspend(*proc->thread);
            
            // Move to back of queue for next round if suspended
            // After suspending, move to back of queue
            linked_list_remove(&scheduler_state->priority_medium, proc);
            linked_list_push_tail(&scheduler_state->priority_medium, proc);
            current++;

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
            spthread_suspend(*proc->thread);
            
            // Move to back of queue for next round if suspended
            // After suspending, move to back of queue
            linked_list_remove(&scheduler_state->priority_low, proc);
            linked_list_push_tail(&scheduler_state->priority_low, proc);
            current++;
            return;
        }
        // Check if we have any runnable processes left
        if (!has_runnable_processes()) {
            LOG_INFO("No more runnable processes, scheduler exiting");
            return;
        }
    }
    current++;
}

void log_all_processes() {
    
}
