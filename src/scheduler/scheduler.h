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
    PROCESS_RUNNING,     // Process is currently executing or ready to execute
    PROCESS_BLOCKED,     // Process is waiting for some event/resource
    PROCESS_STOPPED,     // Process execution has been suspended
    PROCESS_ZOMBIED      // Process has terminated but parent hasn't called wait() to read exit status
} process_state;

typedef enum {
    PRIORITY_HIGH,
    PRIORITY_MEDIUM,
    PRIORITY_LOW
} priority_t;

// Forward declaration
typedef struct pcb_st pcb_t;
typedef linked_list(pcb_t)* pcb_ll_t;

typedef struct child_process_st child_process_t;
typedef linked_list(child_process_t)* child_process_ll_t;

struct child_process_st {
    pcb_t* process;
    child_process_t* next;
    child_process_t* prev;
};


/**
 * Process level file descriptor table
 */

#define PROCESS_FD_TABLE_SIZE 128 
typedef struct process_fd_entry_st
{
    uint16_t global_fd; // file descriptor
    uint32_t offset;
    uint8_t mode; // F_READ, F_WRITE, F_APPEND
    bool in_use;
} process_fd_entry;


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
    child_process_ll_t children;

    // Process level file descriptor table
    process_fd_entry process_fd_table[PROCESS_FD_TABLE_SIZE];

    // Process state
    process_state state;
    pid_t waited_child;

    // Process priority
    priority_t priority;
    double sleep_time;

    // Thread
    spthread_t* thread;
    void* (*func)(void*);
    char* command;
    char** argv;
    int exit_status;
    
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

    // Process count
    unsigned int process_count;
} scheduler_t;

extern scheduler_t* scheduler_state;

void init_scheduler();
void pcb_destructor(void* pcb);

// ================================ Process Management API ================================

void unblock_process(pcb_t* process);
void block_process(pcb_t* process);
void kill_process(pcb_t* process);
void continue_process(pcb_t* process);
void put_process_to_sleep(pcb_t* process, unsigned int ticks);
void cleanup_zombie_children(pcb_t* parent);
void run_scheduler();

// ================================ Kernel-Level Process Management ================================

void k_add_to_ready_queue(pcb_t* process);
bool k_block_process(pcb_t* process);
bool k_unblock_process(pcb_t* process);
void k_proc_exit(pcb_t* process, int exit_status);
void k_yield(void);
bool k_stop_process(pcb_t* process);
bool k_continue_process(pcb_t* process);
bool k_set_priority(pcb_t* process, int priority);
bool k_sleep(pcb_t* process, unsigned int ticks);
void k_get_processes_from_queue(pcb_ll_t queue);
void k_get_all_process_info();
pcb_t* k_get_current_process(void);
pid_t k_waitpid(pid_t pid, int* wstatus, bool nohang);

pcb_t* get_process_by_pid(pid_t pid);

void k_toggle_logging();
void k_print_ps_output();
void k_log(const char *format, ...);

#endif
