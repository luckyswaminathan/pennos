#include "scheduler.h"
#include "logger.h"
#include "../../lib/exiting_alloc.h"
#include "../../lib/linked_list.h"
#include "spthread.h"
#include <string.h>
#include "shell/commands.h"



pcb_t* k_proc_create(pcb_t *parent, void* arg) {
    
    pcb_t* proc = (pcb_t*) exiting_malloc(sizeof(pcb_t));
    LOG_INFO("Adding PID %d (priority %d) at address %p", scheduler_state->process_count, proc->priority, proc);
    log_queue_state();
    proc->ppid = parent->pid;
    LOG_INFO("Parent PID %d", proc->ppid);
    proc->pid = scheduler_state->process_count++;
    LOG_INFO("Spawning process %d", proc->pid);
    proc->state = PROCESS_RUNNING;
    if (proc->pid <= 1) {
        // PID 0 and 1 should be high priority
        proc->priority = PRIORITY_HIGH;
    } else {
        proc->priority = PRIORITY_MEDIUM;
    }
    
    proc->children.head = NULL;
    proc->children.tail = NULL;
    proc->children.ele_dtor = NULL;
    proc->child_pointers.prev = NULL;
    proc->child_pointers.next = NULL;
    proc->pgid = parent->pgid;
    
    linked_list_push_tail(&parent->children, proc, child_pointers.prev, child_pointers.next);
    if (arg != NULL) {
        struct command_context* ctx = (struct command_context*)arg;
        char**command = ctx->command;
        proc->command = strdup(*command);
    } else {
        proc->command = "shell";
    }

    linked_list_push_tail(&scheduler_state->processes, proc, process_pointers.prev, process_pointers.next);
    log_queue_state();
    LOG_INFO("AFTER CREATING");
    add_process_to_queue(proc);
    
    // Log process creation
    log_create(proc->pid, proc->priority, proc->command);
    
    return proc;
}

void k_proc_cleanup(pcb_t *proc) {
    LOG_INFO("freeing proc %d", proc->pid);
    log_process_state();
    if (proc->thread) {
        free(proc->thread);
    }
    if (proc->children.head) {
        pcb_t* head_child = proc->children.head;
        while (head_child != NULL) {
            LOG_INFO("freeing child %d", head_child->pid);
            head_child->ppid = 0;
            pcb_t* next_child = head_child->child_pointers.next;
            linked_list_push_tail(&scheduler_state->init->children, head_child, child_pointers.prev, child_pointers.next);
            
            // Log orphaned process
            log_orphan(head_child->pid, head_child->priority, head_child->command);
            
            head_child = next_child;
        }
    }
    free(proc);
}
