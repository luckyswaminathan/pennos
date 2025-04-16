#include <stdio.h>
#include <stdlib.h> 
#include "scheduler.h"
#include "logger.h"
#include "../../lib/exiting_alloc.h"
#include "../../lib/linked_list.h"
#include "spthread.h"
#include <sys/time.h>
#include "kernel.h"
#include <bits/sigaction.h>
#include <asm-generic/signal-defs.h>

scheduler_t* scheduler_state = NULL;
static const int centisecond = 10000;

/**
 * @brief PCB destructor function for linked lists
 * 
 * This function is used as a destructor for PCBs in linked lists.
 * It frees the memory allocated for the PCB and its associated resources.
 * 
 * @param pcb Pointer to the PCB to destroy
 */
void pcb_destructor(void* pcb) {
    pcb_t* pcb_ptr = (pcb_t*)pcb;
    if (pcb_ptr != NULL) {
        // Free the children list if it exists
        if (pcb_ptr->children != NULL) {
            linked_list_clear(pcb_ptr->children);
            free(pcb_ptr->children);
        }
        
        // Free the thread if it exists
        if (pcb_ptr->thread != NULL) {
            // Properly join the thread to clean up its resources
            spthread_join(*pcb_ptr->thread, NULL);
            free(pcb_ptr->thread);
        }
        
        // Free command and argv if they exist
        if (pcb_ptr->command != NULL) {
            free(pcb_ptr->command);
        }
        
        if (pcb_ptr->argv != NULL) {
            for (int i = 0; pcb_ptr->argv[i] != NULL; i++) {
                free(pcb_ptr->argv[i]);
            }
            free(pcb_ptr->argv);
        }
        
        // Free the PCB itself
        free(pcb_ptr);
    }
}

/**
 * @brief Initialize the init process
 */
static void _init_init_process() {
    scheduler_state->init_process = (pcb_t*)malloc(sizeof(pcb_t));
    scheduler_state->init_process->pid = 0;
    scheduler_state->init_process->ppid = 0;
    scheduler_state->init_process->pgid = 0;
    
    // Initialize children list directly
    scheduler_state->init_process->children = exiting_malloc(sizeof(linked_list(pcb_t)));
    scheduler_state->init_process->children->head = NULL;
    scheduler_state->init_process->children->tail = NULL;
    scheduler_state->init_process->children->ele_dtor = pcb_destructor;
    
    scheduler_state->init_process->state = PROCESS_RUNNING;
    scheduler_state->init_process->priority = PRIORITY_HIGH;
    scheduler_state->init_process->sleep_time = 0;
    scheduler_state->init_process->thread = NULL;
    scheduler_state->init_process->func = NULL;
}

/**
 * @brief Signal handler for SIGALRM
 * 
 * This function is used to handle the SIGALRM signal.
 * It is used to trigger the scheduler by sending SIGALRM every 100ms
 */
static void alarm_handler(int signum) {}

/**
 * @brief Set up the signal handler for SIGALRM
 * 
 * It is used to trigger the scheduler by sending SIGALRM every 100ms
 */
static void _setup_sigalarm() {
    // Set up signal handler for SIGALRM
    sigset_t suspend_set;
    sigfillset(&suspend_set);
    sigdelset(&suspend_set, SIGALRM);

    // To make sure that SIGALRM doesn't terminate the process
    struct sigaction act = (struct sigaction){
        .sa_handler = alarm_handler,
        .sa_mask = suspend_set,
        .sa_flags = SA_RESTART,
    };
    sigaction(SIGALRM, &act, NULL);

    // Unblock SIGALRM on the thread
    sigset_t alarm_set;
    sigemptyset(&alarm_set);
    sigaddset(&alarm_set, SIGALRM);
    pthread_sigmask(SIG_UNBLOCK, &alarm_set, NULL);
}

/**
 * @brief Initialize the scheduler
 */
void init_scheduler() {
    scheduler_state = (scheduler_t*)exiting_malloc(sizeof(scheduler_t));

    // Initialize priority queues
    for (int i = 0; i < 3; i++) {
        scheduler_state->ready_queues[i].head = NULL;
        scheduler_state->ready_queues[i].tail = NULL;
        scheduler_state->ready_queues[i].ele_dtor = pcb_destructor;
    }
    
    // Initialize other queues
    scheduler_state->blocked_queue.head = NULL;
    scheduler_state->blocked_queue.tail = NULL;
    scheduler_state->blocked_queue.ele_dtor = pcb_destructor;
    
    scheduler_state->zombie_queue.head = NULL;
    scheduler_state->zombie_queue.tail = NULL;
    scheduler_state->zombie_queue.ele_dtor = pcb_destructor;
    
    scheduler_state->stopped_queue.head = NULL;
    scheduler_state->stopped_queue.tail = NULL;
    scheduler_state->stopped_queue.ele_dtor = pcb_destructor;

    // Initialize init process and put it on the highest priority queue
    _init_init_process();
    linked_list_push_tail(&scheduler_state->ready_queues[PRIORITY_HIGH], scheduler_state->init_process);

    // Initialize current process
    scheduler_state->current_process = scheduler_state->init_process;

    // Initialize ticks
    scheduler_state->ticks = 0;

    // Set up signal handler for SIGALRM to handle the alarm responsible for triggering the scheduler
    _setup_sigalarm();

    // Initialize alarm. Will be used to trigger the scheduler by sending SIGALRM every 100ms
    struct itimerval it;
    it.it_interval = (struct timeval){.tv_usec = centisecond * 10};
    it.it_value = it.it_interval;
    setitimer(ITIMER_REAL, &it, NULL);
}
