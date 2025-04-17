#include "scheduler.h"
#include "kernel.h"
#include "logger.h"
#include <errno.h>
#include "../../lib/exiting_alloc.h"
#include "../../lib/linked_list.h"
#include "spthread.h"
#include <stdlib.h> // For NULL
#include <string.h> // For strcmp etc. if needed

// Forward declaration for the scheduler function (to be implemented later)
void run_scheduler(); 

// Helper function prototypes
static pcb_t* find_process_in_queue(linked_list(pcb_t)* queue, pid_t pid);
static pcb_t* find_process_anywhere(pid_t pid);
static bool remove_process_from_queue(linked_list(pcb_t)* queue, pcb_t* proc);
static bool remove_process_from_ready_queues(pcb_t* proc);


/**
 * @brief Finds a process with the given PID in a specific queue.
 * 
 * @param queue The linked list queue to search.
 * @param pid The PID to search for.
 * @return pcb_t* Pointer to the PCB if found, NULL otherwise.
 */
static pcb_t* find_process_in_queue(linked_list(pcb_t)* queue, pid_t pid) {
    if (!queue) return NULL;
    pcb_t* current = queue->head;
    while (current != NULL) {
        if (current->pid == pid) {
            return current;
        }
        current = current->next;
    }
    return NULL;
}

/**
 * @brief Finds a process with the given PID across all relevant scheduler queues.
 * Searches ready, blocked, and stopped queues. Does NOT search zombie queue.
 * 
 * @param pid The PID to search for.
 * @return pcb_t* Pointer to the PCB if found, NULL otherwise.
 */
static pcb_t* find_process_anywhere(pid_t pid) {
    pcb_t* proc = NULL;
    // Check ready queues
    for (int i = 0; i < 3; i++) {
        proc = find_process_in_queue(&scheduler_state->ready_queues[i], pid);
        if (proc) return proc;
    }
    // Check blocked queue
    proc = find_process_in_queue(&scheduler_state->blocked_queue, pid);
    if (proc) return proc;
    
    // Check stopped queue
    proc = find_process_in_queue(&scheduler_state->stopped_queue, pid);
    if (proc) return proc;

    // Special check for current process if it's not in a queue (shouldn't happen often)
    if (scheduler_state->current_process && scheduler_state->current_process->pid == pid) {
         return scheduler_state->current_process;
    }

    return NULL; // Not found in active queues
}


/**
 * @brief Removes a specific process node from a given queue.
 * Uses pointer comparison.
 * 
 * @param queue The queue to remove from.
 * @param proc The process PCB pointer to remove.
 * @return true if removed, false otherwise.
 */
static bool remove_process_from_queue(linked_list(pcb_t)* queue, pcb_t* proc) {
    // linked_list_remove requires finding the node first, which is inefficient.
    // Assuming linked_list library has or needs a function like:
    // linked_list_remove_node(linked_list(T)* list, T* node_to_remove)
    // For now, we'll manually implement removal logic here for clarity.
    
    if (!queue || !proc || !queue->head) {
        return false;
    }

    if (queue->head == proc) {
        queue->head = proc->next;
        if (queue->tail == proc) {
            queue->tail = NULL;
        }
        if (queue->head) {
            queue->head->prev = NULL;
        }
        proc->next = NULL;
        proc->prev = NULL;
        return true;
    }

    pcb_t* current = queue->head;
    while (current != NULL && current != proc) {
        current = current->next;
    }

    if (current == proc) { // Found the node
        if (current->prev) {
            current->prev->next = current->next;
        }
        if (current->next) {
            current->next->prev = current->prev;
        }
        if (queue->tail == current) {
            queue->tail = current->prev;
        }
        proc->next = NULL;
        proc->prev = NULL;
        return true;
    }

    return false; // Not found
}

/**
 * @brief Removes a process from whichever ready queue it resides in.
 * 
 * @param proc The process to remove.
 * @return true if removed from a ready queue, false otherwise.
 */
static bool remove_process_from_ready_queues(pcb_t* proc) {
    if (!proc) return false;
    for (int i = 0; i < 3; i++) {
        if (remove_process_from_queue(&scheduler_state->ready_queues[i], proc)) {
            return true;
        }
    }
    return false;
}


/**
 * @brief Create a child process that executes the function `func`.
 * The child will retain some attributes of the parent.
 *
 * @param func Function to be executed by the child process.
 * @param arg Argument to be passed to the function.
 * @return pid_t The process ID of the created child process, or -1 on error.
 */
pid_t s_spawn(void *(*func)(void *), void *arg)
{
    log_queue_state(); // Log state before spawn
    if (!scheduler_state || !scheduler_state->current_process) {
         LOG_ERROR("Scheduler or current process not initialized during s_spawn.");
         errno = EAGAIN; // Or another suitable error
         return -1;
    }
    
    pcb_t *parent = scheduler_state->current_process;
    LOG_INFO("s_spawn called by parent %d", parent->pid);

    // k_proc_create handles adding to parent's children list and ready queue
    pcb_t *proc = k_proc_create(parent, arg);
    if (!proc) {
        LOG_ERROR("k_proc_create failed in s_spawn.");
        errno = EAGAIN;
        return -1;
    }

    // Store the function pointer
    proc->func = func;

    // Create the underlying thread using spthread
    proc->thread = (spthread_t *)exiting_malloc(sizeof(spthread_t));
    if (spthread_create(proc->thread, NULL, func, arg) != 0)
    {
        LOG_ERROR("Failed to create spthread for process %d", proc->pid);
        // Cleanup: Need to remove the process we just added
        remove_process_from_ready_queues(proc); 
        // Remove from parent's children list (requires a list find/remove function)
        // linked_list_remove(parent->children, proc); // Assuming this function exists
        // For now, just log the error, memory will leak without list removal.
        // TODO: Implement proper cleanup on thread creation failure
        
        free(proc->thread); 
        proc->thread = NULL;
        // The PCB itself might be freed if parent->children had the destructor set,
        // or it might leak. This depends on linked_list_remove implementation.
        // Let's assume we need to free it manually if not removed from list.
        // free(proc); // Risky without proper list removal first.

        errno = EAGAIN;
        return -1;
    }

    LOG_INFO("s_spawn successful for PID %d", proc->pid);
    log_queue_state(); // Log state after spawn
    return proc->pid;
}


/**
 * @brief Wait on a child of the calling process, until it changes state (zombies).
 * If `nohang` is true, this will not block the calling process and return immediately.
 *
 * @param pid Process ID of the child to wait for (-1 for any child).
 * @param wstatus Pointer to an integer variable where the exit status will be stored.
 * @param nohang If true, return immediately if no child has zombied.
 * @return pid_t The process ID of the zombied child on success, 0 if nohang and no child zombied, -1 on error.
 */
pid_t s_waitpid(pid_t pid, int *wstatus, bool nohang)
{
    pcb_t *current_process = scheduler_state->current_process;
    LOG_INFO("s_waitpid called by PID %d for target PID %d (nohang=%d)", current_process->pid, pid, nohang);
    log_process_state();

    if (!current_process->children || !current_process->children->head) {
        LOG_INFO("PID %d has no children.", current_process->pid);
        errno = ECHILD;
        return -1; // No children
    }

    pcb_t* child_to_wait_on = NULL;
    bool found_zombie = false;

    // First pass: Check for already zombied children matching criteria
    pcb_t* current_child = current_process->children->head;
    pcb_t* zombie_child = NULL;
    while (current_child != NULL) {
        if (pid == -1 || current_child->pid == pid) {
            // Check if this child is in the zombie queue
            zombie_child = find_process_in_queue(&scheduler_state->zombie_queue, current_child->pid);
            if (zombie_child) { // Found a matching zombied child
                 child_to_wait_on = zombie_child;
                 found_zombie = true;
                 break;
            }
        }
        current_child = current_child->next; // Iterate through parent's list of children
    }


    if (found_zombie) {
        LOG_INFO("Found zombied child PID %d for parent PID %d", child_to_wait_on->pid, current_process->pid);
        
        // Log that we're waiting on this process
        log_waited(child_to_wait_on->pid, child_to_wait_on->priority, child_to_wait_on->command);

        // Store exit status if requested
        if (wstatus != NULL) {
            // TODO: Store exit status in PCB during s_exit
            // *wstatus = child_to_wait_on->exit_status; 
             *wstatus = 0; // Placeholder
        }

        pid_t waited_pid = child_to_wait_on->pid;

        // Remove from parent's children list *first*
        // Requires a list function: linked_list_remove_by_value(list, value) or similar
        // bool removed_from_parent = linked_list_remove_node(current_process->children, child_to_wait_on);
        // LOG_INFO("Removed from parent result: %d", removed_from_parent);
        // TODO: Implement proper removal from parent's children list

        // Remove from zombie queue - this triggers the destructor and frees the PCB
        bool removed = remove_process_from_queue(&scheduler_state->zombie_queue, child_to_wait_on);
        if (!removed) {
             LOG_ERROR("Failed to remove PID %d from zombie queue!", waited_pid);
             // This is problematic, indicates inconsistency.
        } else {
             LOG_INFO("Removed PID %d from zombie queue.", waited_pid);
        }
        
        log_queue_state(); // Log state after cleanup
        return waited_pid;
    }

    // No matching zombie child found
    if (nohang) {
        LOG_INFO("nohang=true, no zombied child found for PID %d.", pid);
        return 0; // Return immediately
    }

    // --- Blocking Logic ---
    // If nohang is false, block the current process until a child zombies.
    LOG_INFO("Blocking PID %d waiting for child PID %d.", current_process->pid, pid);
    
    // Remove current process from its ready queue
    if (!remove_process_from_ready_queues(current_process)) {
         LOG_ERROR("Failed to remove current process %d from ready queues for blocking!", current_process->pid);
         // Should not happen if it was running. Continue cautiously.
    }
    
    // Change state and add to blocked queue
    current_process->state = PROCESS_BLOCKED;
    // TODO: Add field to PCB to store PID waited for? current_process->waiting_for_pid = pid;
    linked_list_push_tail(&scheduler_state->blocked_queue, current_process);
    
    log_block(current_process->pid, current_process->priority, current_process->command);
    log_queue_state();

    // Yield control to the scheduler
    // The scheduler (when a process exits/zombies) needs to check if the parent 
    // is blocked waiting for it and unblock it.
    run_scheduler(); // Call scheduler to switch process - IMPLEMENTATION NEEDED

    // --- Execution resumes here after being unblocked ---
    LOG_INFO("PID %d unblocked after waiting.", current_process->pid);
    log_unblock(current_process->pid, current_process->priority, current_process->command);

    // When unblocked, the child should now be a zombie. Retry the logic.
    // This recursive call might be complex; alternatively, the unblocking logic
    // in the scheduler could handle finding the zombie and returning its status.
    // For simplicity, let's try calling waitpid again.
    return s_waitpid(pid, wstatus, false); // Retry the wait, child should be zombie now.
    // Need to be careful about infinite loops if unblocking happens without zombie.
}


/**
 * @brief Send a signal (terminate) to a particular process.
 *
 * @param pid Process ID of the target process.
 * @return 0 on success, -1 on error (e.g., process not found, permission denied).
 */
int s_kill(pid_t pid)
{
    LOG_INFO("s_kill called for PID %d by PID %d", pid, scheduler_state->current_process->pid);

    if (pid <= 0) { // Cannot kill init process or invalid PIDs
        LOG_WARN("Attempted to kill invalid PID %d", pid);
        errno = EINVAL;
        return -1;
    }
    
    if (pid == scheduler_state->current_process->pid) {
        // Handle killing self - should likely call s_exit?
        LOG_INFO("Process %d attempting to kill itself.", pid);
        s_exit(SIGKILL); // Or some defined status for killed by self
        return 0; // s_exit does not return
    }

    pcb_t *proc = find_process_anywhere(pid);
    if (!proc) {
        LOG_WARN("s_kill: Process PID %d not found.", pid);
        errno = ESRCH;
        return -1;
    }
    
    LOG_INFO("Found process PID %d (state %d) to kill.", proc->pid, proc->state);
    log_signaled(proc->pid, proc->priority, proc->command); // Log the signal event

    // 1. Reparent children to init
    k_proc_reparent_children(proc);

    // 2. Cancel the underlying thread (best effort)
    if (proc->thread) {
        spthread_cancel(*(proc->thread));
        // Note: Cancellation is asynchronous. Thread might not stop immediately.
    }

    // 3. Remove from current queue
    bool removed = false;
    if (proc->state == PROCESS_RUNNING) { // Includes ready processes
        removed = remove_process_from_ready_queues(proc);
    } else if (proc->state == PROCESS_BLOCKED) {
        removed = remove_process_from_queue(&scheduler_state->blocked_queue, proc);
    } else if (proc->state == PROCESS_STOPPED) {
        removed = remove_process_from_queue(&scheduler_state->stopped_queue, proc);
    }
    
    if (!removed && proc != scheduler_state->current_process) { // Current process wouldn't be in a queue yet
        LOG_WARN("s_kill: Process PID %d was found but not in expected queues (state %d).", pid, proc->state);
        // This might indicate a state inconsistency. Proceed with caution.
    }

    // 4. Change state to Zombie and add to zombie queue
    proc->state = PROCESS_ZOMBIED;
    // TODO: Set exit status proc->exit_status = W_MAKE_EXIT_STATUS(SIGKILL, 0); // Indicate killed by signal
    linked_list_push_tail(&scheduler_state->zombie_queue, proc);

    LOG_INFO("Process PID %d killed and moved to zombie queue.", pid);
    log_queue_state();

    return 0;
}

/**
 * @brief Change the priority of a process.
 * 
 * @param pid Process ID of the target process.
 * @param priority New priority level (PRIORITY_HIGH, PRIORITY_MEDIUM, PRIORITY_LOW).
 * @return 0 on success, -1 on error.
 */
int s_nice(pid_t pid, int priority)
{
    LOG_INFO("s_nice called for PID %d to priority %d by PID %d", pid, priority, scheduler_state->current_process->pid);

    if (priority < PRIORITY_HIGH || priority > PRIORITY_LOW) {
        LOG_WARN("s_nice: Invalid priority level %d specified.", priority);
        errno = EINVAL;
        return -1;
    }

    if (pid <= 0) { // Cannot nice init process or invalid PIDs
        LOG_WARN("Attempted to nice invalid PID %d", pid);
        errno = EINVAL;
        return -1;
    }
    
    pcb_t *proc = find_process_anywhere(pid);
    if (!proc) {
        LOG_WARN("s_nice: Process PID %d not found.", pid);
        errno = ESRCH;
        return -1;
    }

    if (proc->priority == priority) {
        LOG_INFO("s_nice: Process PID %d already has priority %d.", pid, priority);
        return 0; // No change needed
    }

    int old_priority = proc->priority;
    proc->priority = priority; // Update priority field

    // If the process is currently ready/running, move it to the new ready queue
    if (proc->state == PROCESS_RUNNING) {
        bool removed = remove_process_from_queue(&scheduler_state->ready_queues[old_priority], proc);
        if (removed) {
             linked_list_push_tail(&scheduler_state->ready_queues[priority], proc);
             LOG_INFO("Moved PID %d from ready queue %d to %d.", pid, old_priority, priority);
        } else if (proc != scheduler_state->current_process) { // Current proc might not be in queue if just switched
             LOG_WARN("s_nice: Process PID %d (state RUNNING) not found in ready queue %d.", pid, old_priority);
             // Might be current_process, just update priority field.
        }
    } else {
        // If blocked or stopped, just update the priority field. It stays in its current queue.
        LOG_INFO("s_nice: Updated priority for blocked/stopped process PID %d to %d.", pid, priority);
    }

    log_nice(pid, old_priority, priority, proc->command);
    log_queue_state();

    return 0;
}


/**
 * @brief Send a stop signal to a process (moves to stopped queue).
 *
 * @param pid Process ID of the target process.
 * @return 0 on success, -1 on error.
 */
int s_stop(pid_t pid)
{
    LOG_INFO("s_stop called for PID %d by PID %d", pid, scheduler_state->current_process->pid);
    
    if (pid <= 0) { // Cannot stop init process or invalid PIDs
        LOG_WARN("Attempted to stop invalid PID %d", pid);
        errno = EINVAL;
        return -1;
    }

    if (pid == scheduler_state->current_process->pid) {
        // Handle stopping self? Doesn't make much sense for cooperative model.
        LOG_WARN("Process %d attempting to stop itself - operation ignored.", pid);
        errno = EINVAL; // Or perhaps just return 0?
        return -1; 
    }
    
    pcb_t *proc = find_process_anywhere(pid);
    if (!proc) {
        LOG_WARN("s_stop: Process PID %d not found.", pid);
        errno = ESRCH;
    return -1;
    }

    if (proc->state == PROCESS_STOPPED || proc->state == PROCESS_ZOMBIED) {
        LOG_INFO("s_stop: Process PID %d is already stopped or zombied.", pid);
        return 0; // Already stopped/zombie, consider success?
    }

    log_stop(proc->pid, proc->priority, proc->command); // Log the stop event

    bool removed = false;
    if (proc->state == PROCESS_RUNNING) { // Includes ready
        removed = remove_process_from_ready_queues(proc);
    } else if (proc->state == PROCESS_BLOCKED) {
        removed = remove_process_from_queue(&scheduler_state->blocked_queue, proc);
    }

    if (!removed) {
         LOG_WARN("s_stop: Process PID %d was found but not in expected queues (state %d).", pid, proc->state);
         // State inconsistency? Proceed cautiously.
    }
    
    proc->state = PROCESS_STOPPED;
    linked_list_push_tail(&scheduler_state->stopped_queue, proc);

    LOG_INFO("Process PID %d stopped and moved to stopped queue.", pid);
    log_queue_state();

    return 0;
}

/**
 * @brief Send a continue signal to a stopped process (moves to ready queue).
 *
 * @param pid Process ID of the target process.
 * @return 0 on success, -1 on error.
 */
int s_cont(pid_t pid)
{
    LOG_INFO("s_cont called for PID %d by PID %d", pid, scheduler_state->current_process->pid);

     if (pid <= 0) { 
        LOG_WARN("Attempted to cont invalid PID %d", pid);
        errno = EINVAL;
        return -1;
    }

    // Search *only* in the stopped queue
    pcb_t *proc = find_process_in_queue(&scheduler_state->stopped_queue, pid);
    
    if (!proc) {
        // Check if it exists but isn't stopped
        pcb_t* other_proc = find_process_anywhere(pid);
        if (other_proc) {
             LOG_INFO("s_cont: Process PID %d exists but is not stopped (state %d).", pid, other_proc->state);
             return 0; // Not an error, process is already running/ready/blocked
        } else {
             LOG_WARN("s_cont: Process PID %d not found.", pid);
             errno = ESRCH;
             return -1;
        }
    }

    // Found in stopped queue
    log_continue(proc->pid, proc->priority, proc->command); // Log the continue event

    // Remove from stopped queue
    bool removed = remove_process_from_queue(&scheduler_state->stopped_queue, proc);
     if (!removed) {
         LOG_ERROR("s_cont: Failed to remove PID %d from stopped queue!", pid);
         // State inconsistency, but proceed.
    }
    
    // Set state to running and add to appropriate ready queue
    proc->state = PROCESS_RUNNING;
    linked_list_push_tail(&scheduler_state->ready_queues[proc->priority], proc);

    LOG_INFO("Process PID %d continued and moved to ready queue %d.", pid, proc->priority);
    log_queue_state();

    return 0;
}

/**
 * @brief Unconditionally exit the calling process.
 * @param status The exit status code.
 */
void s_exit(int status)
{
    pcb_t* current_process = scheduler_state->current_process;
    LOG_INFO("s_exit called by PID %d with status %d", current_process->pid, status);

    if (current_process->pid == 0) {
        LOG_ERROR("Init process (PID 0) attempted to exit!");
        // What should happen? Maybe loop indefinitely or trigger kernel panic?
        // For now, just prevent it from proceeding with exit logic.
        while(1) { /* Loop forever? */ } 
    }

    // 1. Reparent children
    k_proc_reparent_children(current_process);

    // 2. Set state, store exit status
    current_process->state = PROCESS_ZOMBIED;
    // TODO: Add exit_status field to pcb_t
    // current_process->exit_status = status; 

    // 3. Remove from ready queue (shouldn't be in one, but safety check)
    remove_process_from_ready_queues(current_process); 
    
    // 4. Add to zombie queue
    linked_list_push_tail(&scheduler_state->zombie_queue, current_process);

    log_exit(current_process->pid, status); // Log exit event
    log_queue_state();

    // 5. Stop executing this thread and trigger scheduler
    // The thread resources will be cleaned up by spthread_destroy called from 
    // the pcb_destructor when the process is removed from the zombie queue by waitpid.

    // Need to yield control - scheduler must run next.
    // The thread needs to terminate itself *after* scheduler picks a new one.
    // This is tricky. Maybe signal the scheduler? Or call run_scheduler indirectly?
    
    // Option A: Call scheduler, which should NOT return here.
    // run_scheduler(); 
    // fprintf(stderr, "ERROR: run_scheduler returned to s_exit for PID %d!\n", current_process->pid);
    // exit(1); // Should not happen

    // Option B: Use spthread_exit which cleans up and terminates the thread.
    // The scheduler must be invoked somehow after this thread terminates.
    // Perhaps the SIGALRM will eventually trigger it, or the unblocking logic
    // in waitpid needs to trigger it.
    spthread_exit((void*)(intptr_t)status); // Exit the underlying thread

    // Should not reach here
    LOG_ERROR("s_exit: Reached code after spthread_exit for PID %d!", current_process->pid);
    while(1); // Loop forever if spthread_exit fails?
}

// void s_sleep(unsigned int ticks) {
//     // TODO: Implement s_sleep
//     // Move current process to blocked queue
//     // Set current_process->sleep_time = ticks + scheduler_state->ticks; 
//     // Call run_scheduler();
// }
