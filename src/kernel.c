#include "scheduler.h"
#include "logger.h"
#include "../shell/exiting_alloc.h"
#include "../lib/linked_list.h"
#include "spthread.h"




pcb_t* k_proc_create(pcb_t *parent) {
    pcb_t* proc = (pcb_t*) exiting_malloc(sizeof(pcb_t));

    proc->ppid = parent->pid;
    proc->pid = scheduler_state->process_count++;
    LOG_INFO("Spawning process %d", proc->pid);
    proc->priority = PRIORITY_MEDIUM;
    proc->state = PROCESS_READY;
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