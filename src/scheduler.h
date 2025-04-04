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
    PROCESS_NEW,     
    PROCESS_READY,
    PROCESS_RUNNING, 
    PROCESS_BLOCKED, 
    PROCESS_TERMINATED 
} process_state;

// --- Process Control Block ---
// Must have 'prev' and 'next' pointers for linked_list.h
typedef struct process_control_block {
    pid_t pid;        
    pid_t ppid;        
    linked_list(pcb_t) children;     
    pid_t pgid;          
    bool is_leader; 
    int fd0;
    int fd1;
    process_state state;        

    spthread_t thread;
    struct process_control_block* prev;
    struct process_control_block* next;

} pcb_t;

typedef struct scheduler {
    linked_list(pcb_t) processes;
    linked_list(pcb_t) priority_high;
    linked_list(pcb_t) priority_medium;
    linked_list(pcb_t) priority_low;
    linked_list(pcb_t) blocked_processes;
    linked_list(pcb_t) terminated_processes;
    int process_count;
} scheduler_t;


#endif
