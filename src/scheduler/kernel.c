#include "scheduler.h"
#include "logger.h"
#include "../../lib/exiting_alloc.h"
#include "../../lib/linked_list.h"
#include "spthread.h"
#include <string.h>
#include "shell/commands.h"
#include <stdlib.h> // For malloc/free
#include <stdio.h> // For debugging


// Counter for assigning unique PIDs
static pid_t next_pid = 1; // PID 0 is reserved for the init process

pcb_t* k_proc_create(pcb_t *parent, void* arg) {
    
    pcb_t* proc = (pcb_t*) exiting_malloc(sizeof(pcb_t));

    // Initialize PCB fields
    proc->pid = next_pid++;
    proc->ppid = parent->pid;
    proc->pgid = (parent->pid == 0) ? proc->pid : parent->pgid; // New process group for children of init, otherwise inherit

    // Allocate and initialize the children list for this new process
    proc->children = (linked_list(pcb_t)*)exiting_malloc(sizeof(linked_list(pcb_t)));
    proc->children->head = NULL;
    proc->children->tail = NULL;
    proc->children->ele_dtor = pcb_destructor; // Children list should also use the destructor

    proc->state = PROCESS_RUNNING;
    proc->sleep_time = 0;
    proc->thread = NULL; // Thread will be created by s_spawn
    proc->func = NULL;   // Function pointer will be set by s_spawn or similar
    proc->argv = NULL;   // Will be set if created from exec-like command

    // Set priority (handle init/shell specially if needed - PID 0 already handled at init)
    // Spec says PID 0 (init) and PID 1 (shell?) should be high priority.
    if (proc->pid == 1) { // Assuming PID 1 is the first shell
        proc->priority = PRIORITY_HIGH;
    } else {
        proc->priority = PRIORITY_MEDIUM; // Default priority
    }

    // Initialize linked list pointers
    proc->prev = NULL;
    proc->next = NULL;
    
    // Add this process to the parent's children list
    // Ensure parent's children list is initialized
    if (parent->children == NULL) {
         // This should not happen if parent is init_process or created by k_proc_create
         LOG_ERROR("Parent PID %d has NULL children list!", parent->pid);
         // Handle error appropriately, maybe allocate it? For now, log and continue cautiously.
         parent->children = (linked_list(pcb_t)*)exiting_malloc(sizeof(linked_list(pcb_t)));
         parent->children->head = NULL;
         parent->children->tail = NULL;
         parent->children->ele_dtor = pcb_destructor;
    }
    linked_list_push_tail(parent->children, proc);
    
    // Handle command name based on argument (e.g., from shell command context)
    if (arg != NULL) {
        // Assuming arg is struct command_context* as before
        // We need shell/commands.h for this structure definition
        struct command_context* ctx = (struct command_context*)arg;
        // Assuming command_context has char** command
        if (ctx->command && ctx->command[0]) {
             proc->command = strdup(ctx->command[0]);
             // Potentially copy argv as well if needed later
        } else {
             proc->command = strdup("unknown"); // Fallback if command missing
        }
    } else {
        // Default command name if no arg provided (e.g., init process creates shell?)
        proc->command = strdup("process"); 
        if(proc->pid == 1) { // Special case for shell likely created by init
            free(proc->command);
            proc->command = strdup("shell");
        }
    }

    // Add the new process to the appropriate ready queue
    linked_list_push_tail(&scheduler_state->ready_queues[proc->priority], proc);
    
    // Log process creation
    LOG_INFO("Created process %d (parent %d, priority %d, cmd '%s')", proc->pid, proc->ppid, proc->priority, proc->command);
    log_create(proc->pid, proc->priority, proc->command);
    log_queue_state(); // Log queue state after adding

    return proc;
}


/**
 * @brief Reparents the children of a terminating process to the init process.
 * This function should be called before the process PCB is finally destroyed.
 * It does NOT free the proc PCB itself.
 *
 * @param proc The process whose children need reparenting.
 */
void k_proc_reparent_children(pcb_t *proc) {
    if (!proc || !proc->children || !scheduler_state || !scheduler_state->init_process) {
        LOG_ERROR("Invalid arguments to k_proc_reparent_children");
        return;
    }

    LOG_INFO("Reparenting children of terminating process %d", proc->pid);
    log_process_state(); // Log state before reparenting

    pcb_t* init_proc = scheduler_state->init_process;
    
    // Use linked_list_pop_head to safely iterate and remove from the old list
    pcb_t* child = NULL;
    while ((child = linked_list_pop_head(proc->children)) != NULL) {
        LOG_INFO("Reparenting child %d to init process %d", child->pid, init_proc->pid);
        child->ppid = init_proc->pid;

        // Add child to init process's children list
        if (init_proc->children == NULL) {
            // Safety check, should be initialized in _init_init_process
            LOG_ERROR("Init process %d has NULL children list!", init_proc->pid);
            // Allocate? This indicates a deeper issue.
            init_proc->children = (linked_list(pcb_t)*)exiting_malloc(sizeof(linked_list(pcb_t)));
            init_proc->children->head = NULL;
            init_proc->children->tail = NULL;
            init_proc->children->ele_dtor = pcb_destructor;
        }
        linked_list_push_tail(init_proc->children, child);
        
        // Log orphaned process
        log_orphan(child->pid, child->priority, child->command ? child->command : "unknown");
    }

    // The original children list structure is now empty, free it.
    // The child PCBs themselves are now owned by the init_process's children list.
    free(proc->children);
    proc->children = NULL; // Avoid dangling pointer

    LOG_INFO("Finished reparenting children of process %d", proc->pid);
    // Note: We do NOT free(proc) here. That's handled by the list destructor later.
}
