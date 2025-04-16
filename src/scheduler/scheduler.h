#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <signal.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/time.h>
#include <sys/types.h>

#include "../../lib/linked_list.h" 

#include "./spthread.h" 

// Will have 3 queues for RUNNING (based on priority), and one for every other state
typedef enum {   
    PROCESS_RUNNING, 
    PROCESS_BLOCKED, 
    PROCESS_STOPPED,
    PROCESS_ZOMBIED
} process_state;

typedef enum {
    PRIORITY_HIGH,
    PRIORITY_MEDIUM,
    PRIORITY_LOW
} priority_t;

// Forward declaration
typedef struct pcb_st pcb_t;

/*
    Process Control Block
    - Process identification
    - File descriptors
    - Process state
    - Process priority
    - Sleep time
    - Thread
*/
struct pcb_st {
    // Process identification
    pid_t pid;        
    pid_t ppid;        
    pid_t pgid;          
    linked_list(pcb_t)* children;

    // File descriptors (may add moree)
    int fd0;
    int fd1;

    // Process state
    process_state state;

    // Process priority
    priority_t priority;
    double sleep_time;

    // Thread
    spthread_t* thread;
    void* (*func)(void*);
    char* command;
    char** argv;
    
    // Linked list pointers (for scheduler queues)
    pcb_t* prev;
    pcb_t* next;
};


typedef struct scheduler {
    // Priority queues (0 = highest priority, 2 = lowest)
    linked_list(pcb_t) ready_queues[3];  // Ready processes by priority

    // Other queues
    linked_list(pcb_t) blocked_queue;
    linked_list(pcb_t) zombie_queue;
    linked_list(pcb_t) stopped_queue;

    // Process count
    unsigned int ticks;

    // Initial and current process
    pcb_t* init_process;
    pcb_t* current_process;
} scheduler_t;

extern scheduler_t* scheduler_state;

void init_scheduler();

#endif
