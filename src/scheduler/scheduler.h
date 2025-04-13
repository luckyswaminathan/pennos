#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <signal.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/time.h>
#include <sys/types.h> // For pid_t

#include "../../lib/linked_list.h" 

#include "./spthread.h" 

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
typedef struct process_control_block pcb_t;

// --- Process Control Block ---
// Must have 'prev' and 'next' pointers for linked_list.h

struct pointer_pair {
    struct process_control_block* prev;
    struct process_control_block* next;
};
struct process_control_block {
    pid_t pid;        
    pid_t ppid;        
    pid_t pgid;          
    bool is_leader; 
    int fd0;
    int fd1;
    process_state state;        
    priority_t priority;   
    double sleep_time;
    char* command;

    spthread_t* thread;
    struct pointer_pair priority_pointers;
    struct pointer_pair process_pointers;
    struct pointer_pair child_pointers;
    linked_list(pcb_t) children;
    char** argv;
};

typedef struct scheduler {
    linked_list(pcb_t) processes;
    linked_list(pcb_t) priority_high;
    linked_list(pcb_t) priority_medium;
    linked_list(pcb_t) priority_low;
    linked_list(pcb_t) blocked_processes;
    linked_list(pcb_t) terminated_processes;
    linked_list(pcb_t) sleeping_processes;
    int process_count;
    pcb_t* init;
    pcb_t* curr;
} scheduler_t;

extern scheduler_t* scheduler_state;

void init_scheduler();
void log_queue_state();
void run_scheduler();
void block_process(pcb_t* proc);
void log_process_state();
void add_process_to_queue(pcb_t* proc);
void put_process_to_sleep(pcb_t* proc, unsigned int ticks);

// Handle orphaned processes by transferring them to init
void handle_orphaned_processes(pcb_t* terminated_process);

void cleanup_zombie_children(pcb_t* parent);

void terminate_process(pcb_t* process);

void unblock_process(pcb_t* proc);

// Functions for stopping and continuing processes
void stop_process(pcb_t* proc);
void continue_process(pcb_t* proc);

void run_next_process();
void log_all_processes();

#endif
