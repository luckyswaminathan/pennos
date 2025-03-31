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
    // --- Linked List pointers (MUST be named prev/next) ---
    struct process_control_block* prev;
    struct process_control_block* next;

    // --- Process Info ---
    spthread_t thread;       // The underlying spthread
    pid_t pid;              // Process ID (you'll need to assign these)
    pid_t ppid;             // Parent Process ID
    pid_t pgid;             // Process Group ID
    process_state state;    // Current state
    int priority;           // Priority (for future use)

    // Add other fields from your comments as needed later
} pcb_t;

// --- Queue Type based on linked_list.h ---
// Defines a struct { pcb_t *head; pcb_t *tail; destroy_fn ele_dtor; }
typedef linked_list(pcb_t) queue_t;

// --- Process Tree (Placeholder) ---
typedef struct process_tree {
    pcb_t* root;           // Root process
    size_t process_count;
    // Define tree structure later if needed
} process_tree_t;

// --- Scheduler State ---
typedef struct {
    process_tree_t* process_tree; // Optional process tree
    pcb_t* running;              // Currently running process PCB
    queue_t ready_queue;         // Queue of ready processes (using linked_list)
    queue_t blocked_queue;       // Queue of blocked processes (using linked_list)
    // Add volatile sig_atomic_t preempt_flag; if needed for signal handler
} scheduler_t;

// --- Function Prototypes ---

// Initialization
void init_scheduler(scheduler_t* scheduler);
void start_scheduling_timer(void); // Sets up SIGALRM timer

// Core scheduling logic (called internally)
void schedule(void); // Decides and dispatches the next process

// Process management API (called by threads or setup code)
void scheduler_add_process(pcb_t* pcb); // Add a new process
void scheduler_exit(void);             // Current process exits
void scheduler_yield(void);            // Current process yields CPU
void scheduler_block(void);            // Current process blocks
void scheduler_unblock(pcb_t* pcb);    // Unblock a specific process

// Main loop to run the scheduler (if needed, maybe integrated elsewhere)
void run_scheduler_loop(void);

#endif // SCHEDULER_H
