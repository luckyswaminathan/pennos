#include "scheduler.h"
#include "logger.h"
#include "../../lib/exiting_alloc.h"
#include "../../lib/linked_list.h"
#include "spthread.h"




pcb_t* k_proc_create(pcb_t *parent, int fd0, int fd1, char **argv) {
    pcb_t* proc = (pcb_t*) exiting_malloc(sizeof(pcb_t));
    LOG_INFO("Adding PID %d (priority %d) at address %p", scheduler_state->process_count, proc->priority, proc);
    log_queue_state();
    proc->ppid = parent->pid;
    LOG_INFO("Parent PID %d", proc->ppid);
    proc->pid = scheduler_state->process_count++;
    LOG_INFO("Spawning process %d", proc->pid);
    if (proc->pid <= 1) {
        // PID 0 and 1 should be high priority
        proc->priority = PRIORITY_HIGH;
    } else {
        proc->priority = PRIORITY_MEDIUM;
    }
    proc->state = PROCESS_READY;
    proc->children.head = NULL;
    proc->children.tail = NULL;
    proc->children.ele_dtor = NULL;
    proc->fd0 = fd0;
    proc->fd1 = fd1;
    proc->argv = argv;
    

    linked_list_push_tail(&scheduler_state->processes, proc, process_pointers.prev, process_pointers.next);
    log_queue_state();
    LOG_INFO("AFTER CREATING");
    add_process_to_queue(proc);
    return proc;
}

void k_proc_cleanup(pcb_t *proc) {
    LOG_INFO("freeing proc %d", proc->pid);
    if (proc->thread) {
        free(proc->thread);
    }
    free(proc);
}
