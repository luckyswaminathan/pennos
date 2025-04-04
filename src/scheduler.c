#include <stdio.h>
#include <stdlib.h> 
#include "scheduler.h"
#include "logger.h"
#include "../shell/exiting_alloc.h"
#include "../lib/linked_list.h"
#include "spthread.h"


scheduler_t* scheduler_state = NULL;

void init_scheduler() {
    scheduler_state = (scheduler_t*) exiting_malloc(sizeof(scheduler_t));
    init_logger("scheduler.log");
    scheduler_state->process_count = 0;
    
    // Initialize all linked lists
    scheduler_state->processes.head = NULL;
    scheduler_state->processes.tail = NULL;
    scheduler_state->processes.ele_dtor = NULL;
    
    scheduler_state->priority_high.head = NULL;
    scheduler_state->priority_high.tail = NULL;
    scheduler_state->priority_high.ele_dtor = NULL;
    
    scheduler_state->priority_medium.head = NULL;
    scheduler_state->priority_medium.tail = NULL;
    scheduler_state->priority_medium.ele_dtor = NULL;
    
    scheduler_state->priority_low.head = NULL;
    scheduler_state->priority_low.tail = NULL;
    scheduler_state->priority_low.ele_dtor = NULL;
    
    scheduler_state->blocked_processes.head = NULL;
    scheduler_state->blocked_processes.tail = NULL;
    scheduler_state->blocked_processes.ele_dtor = NULL;
    
    scheduler_state->terminated_processes.head = NULL;
    scheduler_state->terminated_processes.tail = NULL;
    scheduler_state->terminated_processes.ele_dtor = NULL;

    pcb_t* init = exiting_malloc(sizeof(pcb_t));
    init->ppid = 0;
    init->pid = scheduler_state->process_count++;
    init->pgid = init->pid;
    init->fd0 = -1;
    init->fd1 = -1;
    init->is_leader = true;
    init->priority = PRIORITY_MEDIUM;
    init->state = PROCESS_RUNNING;
    spthread_t* thread = (spthread_t*)exiting_malloc(sizeof(spthread_t));
    init->thread = *thread;
    spthread_create(thread, NULL, NULL, NULL);
    
    scheduler_state->init = init;
}
pcb_t* k_proc_create(pcb_t *parent) {
    pcb_t* proc = (pcb_t*) exiting_malloc(sizeof(pcb_t));

    proc->ppid = parent->pid;
    proc->pid = scheduler_state->process_count++;
    LOG_INFO("Spawning process %d", proc->pid);
    proc->priority = PRIORITY_MEDIUM;
    proc->children.head = NULL;
    proc->children.tail = NULL;
    proc->children.ele_dtor = NULL;
    linked_list_push_tail(&scheduler_state->processes, proc);
    linked_list_push_tail(&scheduler_state->priority_medium, proc);
    return proc;
}

void k_proc_cleanup(pcb_t *proc) {
    linked_list_remove(&scheduler_state->processes, proc);
    linked_list_remove(&scheduler_state->priority_medium, proc);
    free(proc);
}

pid_t s_spawn(void* (*func)(void*), char *argv[], int fd0, int fd1) {
    pcb_t* proc = k_proc_create(scheduler_state->init);
    proc->fd0 = fd0;
    proc->fd1 = fd1;
    proc->argv = argv;
    spthread_create(&proc->thread, NULL, func, NULL);
    return proc->pid;
}

pid_t s_waitpid(pid_t pid, int* wstatus, bool nohang) {
    pcb_t* proc = scheduler_state->processes.head;
    while (proc != NULL) {
        if (proc->pid == pid) {
            if (proc->state == PROCESS_TERMINATED) {
                k_proc_cleanup(proc);
                return pid;
            }
            if (nohang) {
                return -1;
            } else {
                spthread_join(proc->thread, (void**)wstatus);
                k_proc_cleanup(proc);
                return pid;
            }
        }
        proc = proc->next;
    }
    return -1;
}
