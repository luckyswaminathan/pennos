#include "scheduler.h"
#include "logger.h"
#include "../../lib/exiting_alloc.h"
#include "../../lib/linked_list.h"
#include "spthread.h"
#include <string.h>
#include "shell/commands.h"
#include <stdlib.h> // For malloc/free
#include <stdio.h> // For debugging

// PID 0 is reserved for init process (or kernel itself)
// User processes start from PID 1
static pid_t next_pid = 1;


/**
 * @brief Creates a new process control block (PCB) as a child of the given parent.
 *
 * This function allocates memory for a new PCB and initializes its fields.
 * It assigns a unique process ID (PID) starting from 1.
 * The new process inherits certain properties implicitly (like being added to the parent's child list)
 * but specific fields like file descriptors, thread info, and the function to execute
 * must be set by the caller after this function returns.
 * The new process starts in the PROCESS_RUNNING state but is not yet placed in a scheduler queue.
 *
 * @param parent Pointer to the parent process's PCB. If NULL, the process is assumed
 *               to be an initial process (like init) with PPID 0.
 * @return pcb_t* Pointer to the newly created PCB, or NULL if allocation fails.
 */
pcbt* k_proc_create(pcb_t *parent) {
    pcb_t* proc = (pcb_t*) exiting_malloc(sizeof(pcb_t));
    if (!proc) {
        perror("Failed to allocate PCB"); 
        return NULL;
    }

    proc->pid = next_pid++;
    proc->ppid = parent ? parent->pid : 0; // Assume PID 0 for parent if NULL (e.g., init process)
    proc->pgid = proc->pid; // New process starts a new process group

    // Initialize children list (without destructor, cleanup is manual via k_proc_cleanup)
    proc->children = (linked_list(pcb_t)*) exiting_malloc(sizeof(linked_list(pcb_t)));
    if (!proc->children) {
        perror("Failed to allocate children list");
        free(proc);
        return NULL;
    }
    *proc->children = linked_list_new(pcb_t, NULL); 

    // Initialize other fields to defaults
    proc->fd0 = -1; // Will be set by caller (e.g., s_spawn)
    proc->fd1 = -1; // Will be set by caller
    proc->state = PROCESS_RUNNING; // Initial state, will be moved to ready queue
    proc->priority = PRIORITY_MEDIUM; // Default priority
    proc->sleep_time = 0.0;
    proc->thread = NULL; // Will be set by caller
    proc->func = NULL;   // Will be set by caller
    proc->command = NULL;// Will be set by caller
    proc->argv = NULL;   // Will be set by caller
    proc->exit_status = 0; // Default exit status

    // Initialize list pointers
    proc->prev = NULL;
    proc->next = NULL;

    // Add to parent's children list
    if (parent && parent->children) { 
        linked_list_push_tail(parent->children, proc);
    }

    return proc;
}


/**
 * @brief Cleans up resources associated with a terminated or finished process.
 *
 * This function performs the necessary cleanup steps when a process is destroyed:
 * 1. Reparents any orphaned children of the process to the init process (PID 0).
 *    It assumes `scheduler_state` and `scheduler_state->init_process` are accessible.
 * 2. Removes the process from its parent's list of children.
 * 3. Frees allocated resources within the PCB, including the thread structure (if any),
 *    command string, argv array, and the children list structure.
 * 4. Frees the PCB structure itself.
 *
 * Note: This function does *not* handle removing the process from scheduler queues
 * (ready, blocked, etc.) or joining the underlying thread; those actions should
 * occur before calling cleanup.
 *
 * @param proc Pointer to the PCB of the process to clean up.
 */
void k_proc_cleanup(pcb_t *proc) {
    if (!proc) {
        return;
    }

    // 1. Reparent children to init process
    // Assumes scheduler_state and scheduler_state->init_process are valid and accessible
    if (scheduler_state && scheduler_state->init_process && proc->children) {
        pcb_t *child;
        // Pop children one by one, reparent, and add to init's list
        while ((child = linked_list_pop_head(proc->children)) != NULL) {
            if (scheduler_state->init_process->children) {
                child->ppid = scheduler_state->init_process->pid;
                linked_list_push_tail(scheduler_state->init_process->children, child);
            }
            // TODO: What if init process children list is NULL? Handle error/log?
        }
        // Free the (now empty) children list structure of the cleaned-up process
        free(proc->children);
        proc->children = NULL;
    }

    // 2. Remove process from its parent's children list (manually unlink)
    // We need get_process_by_pid which is now non-static in scheduler.c
    pcb_t* parent = get_process_by_pid(proc->ppid);
    if (parent && parent->children) {
        linked_list(pcb_t)* children_list = parent->children;
        
        // Manually unlink 'proc' from the 'children_list' without calling destructor
        if (proc->prev) {
            proc->prev->next = proc->next;
        } else {
            // Proc is the head
            children_list->head = proc->next;
        }
        
        if (proc->next) {
            proc->next->prev = proc->prev;
        } else {
            // Proc is the tail
            children_list->tail = proc->prev;
        }

        // Reset proc's pointers after unlinking
        proc->prev = NULL;
        proc->next = NULL;
    }

    // 3. Free process resources (similar to pcb_destructor but without list interactions)
    if (proc->thread) {
        // Note: Joining the thread should happen before cleanup, e.g., in waitpid.
        // Here we just free the spthread_t structure.
        free(proc->thread);
        proc->thread = NULL;
    }
    if (proc->command) {
        free(proc->command);
        proc->command = NULL;
    }
    if (proc->argv) {
        for (int i = 0; proc->argv[i] != NULL; i++) {
            free(proc->argv[i]);
        }
        free(proc->argv);
        proc->argv = NULL;
    }

    // 4. Free the PCB structure itself
    free(proc);
}
