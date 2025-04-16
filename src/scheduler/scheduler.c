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
static int quantum = 0;

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
            spthread_destroy(pcb_ptr->thread);
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
    scheduler_state->init_process->children = (linked_list(pcb_t)*)malloc(sizeof(linked_list(pcb_t)));
    scheduler_state->init_process->state = PROCESS_RUNNING;
    scheduler_state->init_process->priority = PRIORITY_HIGH;
    scheduler_state->init_process->sleep_time = 0;
    scheduler_state->init_process->thread = NULL;
    scheduler_state->init_process->func = NULL;
}

/**
 * @brief Initialize a PCB queue
 * 
 * @param queue The PCB queue to initialize
 */
static void _init_queues(linked_list(pcb_t)* queue) {
    queue->head = NULL;
    queue->tail = NULL;
    queue->ele_dtor = pcb_destructor;
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
 * This function is used to set up the signal handler for SIGALRM.
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
static void init_scheduler() {
    scheduler_state = (scheduler_t*)exiting_malloc(sizeof(scheduler_t));

    // Initialize priority queues
    for (int i = 0; i < 3; i++) {
        _init_queues(&scheduler_state->ready_queues[i]);
    }
    // Initialize other queues
    _init_queues(&scheduler_state->blocked_queue);
    _init_queues(&scheduler_state->zombie_queue);
    _init_queues(&scheduler_state->stopped_queue);

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