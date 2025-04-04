#include <stdio.h>
#include "scheduler.h"
#include "logger.h"
#include "../lib/linked_list.h"



scheduler_t* scheduler_state = NULL;

void init_scheduler() {
    scheduler_state = (scheduler_state_t*) exiting_malloc(sizeof(scheduler_state_t));
    scheduler_state->process_count = 0;
    scheduler_state->processes = (linked_list(pcb_t)) exiting_malloc(sizeof(linked_list(pcb_t)));
    init_linked_list(scheduler_state->processes);
    
}
pcb_t* k_proc_create(pcb_t *parent) {
    pcb_t* proc = (pcb_t*) exiting_malloc(sizeof(pcb_t));

    proc->ppid = parent->pid;
    proc->pid = scheduler_state->process_count++;
    proc->prev = parent;
    proc->next = NULL;
    proc->priority = PRIORITY_MEDIUM;
    parent->next = proc;
    linked_list_push_tail(scheduler_state->processes, proc);
    return proc;
}

void k_proc_cleanup(pcb_t *proc) {
    proc->prev->next = proc->next;
    if (proc->next) {
        proc->next->prev = proc->prev;
    }
    free(proc);
}
