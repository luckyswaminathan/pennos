#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <signal.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/time.h>
#include <sys/types.h> // For pid_t

#include "../lib/linked_list.h" 

#include "./spthread.h" 

typedef enum {
    PROCESS_NEW,      // Just created, not yet ready
    PROCESS_READY,    // Ready to run
    PROCESS_RUNNING,  // Currently executing
    PROCESS_BLOCKED,  // Waiting for an event
    PROCESS_TERMINATED // Finished execution
} process_state;

// --- Process Control Block ---
// Must have 'prev' and 'next' pointers for linked_list.h
typedef struct process_control_block {
    // Process identification
    pid_t pid;              // Process ID
    pid_t ppid;             // Parent process ID
    int job_id;             // Job ID for shell
    pid_t pgid;             // Process group ID
    bool is_leader;         // Process group leader flag

    // Thread info
    spthread_t thread;
    process_state state;

    // Scheduling info
    int priority;
    unsigned long start_time;
    unsigned long cpu_time;

    // Signal handling
    sigset_t pending_signals;

    // Shell/Job control
    char* command;          // Command string
    bool is_background;     // Background process?
    int exit_status;        // Exit status when terminated

    // Linked list pointers (for ready/blocked queues)
    struct process_control_block* prev;
    struct process_control_block* next;

    // Children list (siblings share same parent)
    struct process_control_block* first_child;    // First child process
    struct process_control_block* next_sibling;   // Next sibling in children list
} pcb_t;

// Scheduler state management
typedef struct scheduler_state {
    // Ready and blocked queues
    pcb_t* ready_head;
    pcb_t* blocked_head;
    pcb_t* current;        // Currently running process

    // Process tracking
    pcb_t* all_processes;  // List of all processes for cleanup
    size_t process_count;

    // Job control
    pcb_t* fg_process;     // Current foreground process
} scheduler_state_t;

void init_scheduler(void);
void run_scheduler(void); 
pcb_t* create_process(spthread_t thread, pid_t ppid, bool is_background);
void make_process_ready(pcb_t* pcb);

#endif
