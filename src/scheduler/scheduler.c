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
static sigset_t suspend_set;

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
static void _setup_sigalarm(sigset_t* suspend_set) {
    // Set up signal handler for SIGALRM
    sigfillset(suspend_set);
    sigdelset(suspend_set, SIGALRM);

    // To make sure that SIGALRM doesn't terminate the process
    struct sigaction act = (struct sigaction){
        .sa_handler = alarm_handler,
        .sa_mask = *suspend_set,
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
    _setup_sigalarm(&suspend_set);

    // Initialize alarm. Will be used to trigger the scheduler by sending SIGALRM every 100ms
    struct itimerval it;
    it.it_interval = (struct timeval){.tv_usec = centisecond * 10};
    it.it_value = it.it_interval;
    setitimer(ITIMER_REAL, &it, NULL);
}

/**
 * @brief Add a process to the appropriate queue based on its priority
 * 
 * This function adds a process to the queue that corresponds to its priority.
 * 
 * @param process The process to add to the queue
 */
void add_process_to_queue(pcb_t* process) {
    linked_list_push_tail(&scheduler_state->ready_queues[process->priority], process);
}

/**
 * @brief Select the next queue to run a process from
 * 
 * This function selects the next queue to run a process from.
 * 
 * @param scheduler_state The scheduler state
 */
int _select_next_queue(scheduler_t* scheduler_state) {
    // Generate a random number between 0 and 18
    int priority_num = quantum % 19;
    if (priority_num < 10 && scheduler_state->ready_queues[PRIORITY_HIGH].head != NULL) {
        // Run 2.25x more often than the lowest priority queue
        return PRIORITY_HIGH;
    } else if (priority_num < 16 && scheduler_state->ready_queues[PRIORITY_MEDIUM].head != NULL) {
        // Run 1.5x more often than the lowest priority queue
        return PRIORITY_MEDIUM;
    } else if (scheduler_state->ready_queues[PRIORITY_LOW].head != NULL) {
        return PRIORITY_LOW;
    } else {
        return -1;
    }
}

/**
 * @brief Update the blocked processes
 * 
 * This function updates the blocked processes by decrementing their sleep time.
 * 
 * @param scheduler_state The scheduler state
 */
void _update_blocked_processes() {
    pcb_t* process = scheduler_state->blocked_queue.head;
    while (process != NULL) {
        // Case 1: Process was sleeping and has now woken up
        if (process->sleep_time > 0) {
            process->sleep_time--;
            if (process->sleep_time == 0) {
                unblock_process(process);
            }
        } else {
            // Case 2: Process was waiting on a child process to exit
            bool all_children_exited = true;
            pcb_t* child = process->children->head;
            while (child != NULL) {
                if (child->state != PROCESS_ZOMBIED) {
                    all_children_exited = false;
                        break;
                }
                child = child->next;
            }
            if (all_children_exited) {
                unblock_process(process);
            }
        }

        process = process->next;
    }
}

/**
 * @brief Run the next process
 * 
 * This function runs the next process from the queue that was selected.
 * 
 * @param scheduler_state The scheduler state
 */
void _run_next_process() {
    // Update the blocked processes before selecting the next process
    _update_blocked_processes();

    // Select the next queue to run a process from
    int next_queue = _select_next_queue(scheduler_state);

    if (next_queue == -1) {
        // No process to run, so we don't consume a quantum
        return;
    }

    // Get the process to run from the queue
    pcb_t* process = (pcb_t*)exiting_malloc(sizeof(pcb_t));
    if (process->thread == NULL) {
        // TODO: Add logging here and check if you should consume a quantum
        return;
    }

    // Set the current process to the process that was just run
    scheduler_state->current_process = process;

    // Run the process and block the scheduler until the next SIGALRM arrives (100ms later)
    spthread_continue(*process->thread);
    sigsuspend(&suspend_set);
    spthread_suspend(*process->thread);
    
    // Consume a quantum
    quantum++;

    // Add the process back to the queue
    linked_list_push_tail(&scheduler_state->ready_queues[next_queue], process);
}

/**
 * @brief Run the scheduler
 * 
 * This function runs the scheduler.
 * 
 * @param scheduler_state The scheduler state
 */
void run_scheduler() {
    while (1) {
        _run_next_process();
    }
}

// ================================ Process Management API ================================

/**
 * @brief Block a process
 * 
 * This function blocks a process by removing it from the queue it is currently on and adding it to the blocked queue.
 * 
 * @param process The process to block
 */
void block_process(pcb_t* process) {
    // Remove the process from the queue it is currently on
    linked_list_remove(&scheduler_state->ready_queues[process->priority], process);

    // Add the process to the blocked queue
    linked_list_push_tail(&scheduler_state->blocked_queue, process);
}

/**
 * @brief Unblock a process
 * 
 * This function unblocks a process by removing it from the blocked queue and adding it to the appropriate queue based on its priority.
 * 
 * @param process The process to unblock
 */
void unblock_process(pcb_t* process) {
    // Remove the process from the blocked queue
    linked_list_remove(&scheduler_state->blocked_queue, process);

    // Add the process to the appropriate queue based on its priority
    linked_list_push_tail(&scheduler_state->ready_queues[process->priority], process);
}

/**
 * @brief Kill a process
 * 
 * This function kills a process by removing it from the queue it is currently on and adding it to the zombie queue.
 * 
 * @param process The process to kill
 */
void kill_process(pcb_t* process) {
    // Remove the process from the queue it is currently on
    linked_list_remove(&scheduler_state->ready_queues[process->priority], process);

    // Add the process to the zombie queue
    linked_list_push_tail(&scheduler_state->zombie_queue, process);
}

/**
 * @brief Continue a process
 * 
 * This function continues a process by removing it from the blocked queue and adding it to the appropriate queue based on its priority.
 * 
 * @param process The process to continue
 */
void continue_process(pcb_t* process) {
    // Remove the process from the blocked queue
    linked_list_remove(&scheduler_state->blocked_queue, process);

    // Add the process to the appropriate queue based on its priority
    linked_list_push_tail(&scheduler_state->ready_queues[process->priority], process);
}

/**
 * @brief Update the priority of a process
 * 
 * This function updates the priority of a process by removing it from the queue it is currently on and adding it to the appropriate queue based on its priority.
 * 
 * @param process The process to update the priority of
 * @param priority The new priority of the process
 */
void update_priority(pcb_t* process, int priority) {
    // Remove the process from the queue it is currently on
    linked_list_remove(&scheduler_state->ready_queues[process->priority], process);

    // Add the process to the appropriate queue based on its priority
    linked_list_push_tail(&scheduler_state->ready_queues[priority], process);
}

/**
 * @brief Put a process to sleep
 * 
 * This function puts a process to sleep by adding it to the blocked queue.
 * 
 * @param process The process to put to sleep
 */
void put_process_to_sleep(pcb_t* process, unsigned int ticks) {
    // Remove the process from the queue it is currently on
    linked_list_remove(&scheduler_state->ready_queues[process->priority], process);

    // Set the sleep time of the process
    process->sleep_time = ticks;

    // Add the process to the blocked queue
    linked_list_push_tail(&scheduler_state->blocked_queue, process);
}

/**
 * @brief Cleanup zombie children
 * 
 * This function cleans up zombie children by removing them from the parent's children list and freeing their resources.
 * 
 */
void cleanup_zombie_children(pcb_t* parent) {
    pcb_t* child = parent->children->head;
    while (child != NULL) {
        pcb_t* next = child->next;
        if (child->state == PROCESS_ZOMBIED) {
            linked_list_remove(parent->children, child);
            free(child);
        }
        child = next;
    }
}

/**
 * @brief Main scheduler function
 * 
 * This function is the main scheduler function that runs in a loop and schedules processes based on their priority.
 * It uses a multi-level feedback queue scheduling algorithm.
 */
int main() {
    init_scheduler();
    run_scheduler();
    return 0;
}
