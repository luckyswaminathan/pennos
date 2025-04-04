#include <stdio.h>
#include "scheduler.h"


pcb_t* k_proc_create(pcb_t *parent) {
    pcb_t* proc = (pcb_t*) exiting_malloc(sizeof(pcb_t));

    proc->ppid = parent->pid;
    proc->prev = parent;
    proc->next = NULL;
    parent->next = proc;

    return proc;
}

void k_proc_cleanup(pcb_t *proc) {
    proc->prev->next = proc->next;
    if (proc->next) {
        proc->next->prev = proc->prev;
    }
    free(proc);
}
