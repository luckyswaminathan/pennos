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
    pid_t pid;        
    pid_t ppid;        
    int job_id;         
    pid_t pgid;          
    bool is_leader;        

    spthread_t thread;
    process_state state;
    bool non_preemptible; 

    int priority;
    unsigned long start_time;
    unsigned long cpu_time;

    sigset_t pending_signals;

    char* command;  
    bool is_background;     
    int exit_status;      

    struct process_control_block* prev;
    struct process_control_block* next;

    struct process_control_block* first_child;
    struct process_control_block* next_sibling;
} pcb_t;

// Priority levels
#define PRIORITY_HIGH 0    
#define PRIORITY_MEDIUM 1 
#define PRIORITY_LOW 2
#define NUM_PRIORITIES 3

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

    // Priority scheduling
    unsigned long quantum_count;           // Total quanta elapsed
    unsigned long priority_quanta[NUM_PRIORITIES];  // Quanta used by each priority level
} scheduler_state_t;


pid_t s_spawn(void* (*func)(void*), char *argv[], int fd0, int fd1);
// pid_t s_waitpid(pid_t pid, int* wstatus, bool nohang);
// int s_kill(pid_t pid, int signal);
// void s_exit(void);

// int s_nice(pid_t pid, int priority);
// void s_sleep(unsigned int ticks);
// pcb_t* k_proc_create(pcb_t *parent);
// void k_proc_cleanup(pcb_t *proc);
#endif

