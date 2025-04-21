#include <stdio.h>
#include <stdlib.h>
#include "scheduler.h"
#include "logger.h"
#include "../../lib/exiting_alloc.h"
#include "../../lib/linked_list.h"
#include "spthread.h"
#include <sys/time.h>
#include "kernel.h"
#include <bits/sigaction.h>
#include <asm-generic/signal-defs.h>
#include <stdbool.h> // Required for bool type

scheduler_t *scheduler_state = NULL;
static const int centisecond = 10000;
static int quantum = 0;
static sigset_t suspend_set;

/**
 * @brief The process to run at each quantum. Randomly disperesed array of 0s, 1s, and 2s (priority levels).
 *
 * This array is used to test the scheduler by running a specific process at each quantum.
 * We have nine 0s, six 1s, and four 2s.
 *
 */
static int process_to_run[19] = {0, 0, 1, 0, 0, 1, 2, 0, 1, 1, 0, 0, 1, 2, 0, 2, 1, 0, 2};

/**
 * @brief PCB destructor function for linked lists
 *
 * This function is used as a destructor for PCBs in linked lists.
 * It frees the memory allocated for the PCB and its associated resources.
 *
 * @param pcb Pointer to the PCB to destroy
 */
void pcb_destructor(void *pcb)
{
    pcb_t *pcb_ptr = (pcb_t *)pcb;
    if (pcb_ptr != NULL)
    {
        // Free the children list if it exists
        if (pcb_ptr->children != NULL)
        {
            linked_list_clear(pcb_ptr->children);
            free(pcb_ptr->children);
        }

        // Free the thread if it exists
        if (pcb_ptr->thread != NULL)
        {
            // Properly join the thread to clean up its resources
            spthread_join(*pcb_ptr->thread, NULL);
            free(pcb_ptr->thread);
        }

        // Free command and argv if they exist
        if (pcb_ptr->command != NULL)
        {
            free(pcb_ptr->command);
        }

        if (pcb_ptr->argv != NULL)
        {
            for (int i = 0; pcb_ptr->argv[i] != NULL; i++)
            {
                free(pcb_ptr->argv[i]);
            }
            free(pcb_ptr->argv);
        }

        // Free the PCB itself
        free(pcb_ptr);
    }
}

/**
 * @brief Signal handler for SIGALRM
 *
 * This function is used to handle the SIGALRM signal.
 * It is used to trigger the scheduler by sending SIGALRM every 100ms
 */
static void alarm_handler(int signum) {}

/**
 * @brief Set up the signal handler for SIGALRM
 *
 * It is used to trigger the scheduler by sending SIGALRM every 100ms
 */
static void _setup_sigalarm(sigset_t *suspend_set)
{
    // Set up signal handler for SIGALRM
    sigfillset(suspend_set);
    sigdelset(suspend_set, SIGALRM);

    // To make sure that SIGALRM doesn't terminate the process
    struct sigaction act = (struct sigaction){
        .sa_handler = alarm_handler,
        .sa_mask = *suspend_set,
        .sa_flags = SA_RESTART,
    };
    sigaction(SIGALRM, &act, NULL);

    // Unblock SIGALRM on the thread
    sigset_t alarm_set;
    sigemptyset(&alarm_set);
    sigaddset(&alarm_set, SIGALRM);
    pthread_sigmask(SIG_UNBLOCK, &alarm_set, NULL);
}

/**
 * @brief Initialize the scheduler
 */
void init_scheduler()
{
    scheduler_state = (scheduler_t *)exiting_malloc(sizeof(scheduler_t));

    // Initialize priority queues
    for (int i = 0; i < 3; i++)
    {
        scheduler_state->ready_queues[i].head = NULL;
        scheduler_state->ready_queues[i].tail = NULL;
        scheduler_state->ready_queues[i].ele_dtor = pcb_destructor;
    }

    // Initialize other queues
    scheduler_state->blocked_queue.head = NULL;
    scheduler_state->blocked_queue.tail = NULL;
    scheduler_state->blocked_queue.ele_dtor = pcb_destructor;

    scheduler_state->zombie_queue.head = NULL;
    scheduler_state->zombie_queue.tail = NULL;
    scheduler_state->zombie_queue.ele_dtor = pcb_destructor;

    scheduler_state->stopped_queue.head = NULL;
    scheduler_state->stopped_queue.tail = NULL;
    scheduler_state->stopped_queue.ele_dtor = pcb_destructor;

    // Initialize ticks
    scheduler_state->ticks = 0;

    // Set up signal handler for SIGALRM to handle the alarm responsible for triggering the scheduler
    _setup_sigalarm(&suspend_set);

    // Initialize alarm. Will be used to trigger the scheduler by sending SIGALRM every 100ms
    struct itimerval it;
    it.it_interval = (struct timeval){.tv_usec = centisecond * 10};
    it.it_value = it.it_interval;
    setitimer(ITIMER_REAL, &it, NULL);
}

/**
 * @brief Adds a process to the appropriate ready queue based on its priority.
 * This is a kernel-level function.
 *
 * @param process The process PCB to add.
 */
void k_add_to_ready_queue(pcb_t *process)
{
    if (!process || !scheduler_state) return; // Basic safety check

    // Ensure priority is within valid range (optional, but good practice)
    if (process->priority < PRIORITY_HIGH || process->priority > PRIORITY_LOW) {
        // Handle error - log? Default to medium? For now, just return.
        fprintf(stderr, "Kernel Error: Process PID %d has invalid priority %d\n", process->pid, process->priority);
        return;
    }
    
    // Use the linked list macro to add to the tail of the correct priority queue
    linked_list_push_tail(&scheduler_state->ready_queues[process->priority], process);
    process->state = PROCESS_RUNNING; // Ensure state reflects it's ready
}

/**
 * @brief Select the next queue to run a process from
 *
 * This function selects the next queue to run a process from.
 *
 * @param scheduler_state The scheduler state
 */
int _select_next_queue(scheduler_t *scheduler_state)
{
    int index = quantum % 19;
    // Uses the process_to_run array to select the next queue to run a process from
    if (scheduler_state->ready_queues[process_to_run[index]].head != NULL)
    {
        return process_to_run[index];
    }
    else
    {
        // If the queue selected is empty, then we select the next queue to run a process from
        // in order of priority
        for (int i = 0; i < 3; i++)
        {
            if (scheduler_state->ready_queues[i].head != NULL)
            {
                return i;
            }
        }
    }
    return -1;
}

/**
 * @brief Update the blocked processes
 *
 * This function updates the blocked processes by decrementing their sleep time.
 *
 * @param scheduler_state The scheduler state
 */
void _update_blocked_processes()
{
    pcb_t *process = scheduler_state->blocked_queue.head;
    while (process != NULL)
    {
        // Case 1: Process was sleeping and has now woken up
        if (process->sleep_time > 0)
        {
            process->sleep_time--;
            if (process->sleep_time == 0)
            {
                unblock_process(process);
            }
        }
        else
        {
            // Case 2: Process was waiting on a child process to exit
            bool all_children_exited = true;
            pcb_t *child = process->children->head;
            while (child != NULL)
            {
                if (child->state != PROCESS_ZOMBIED)
                {
                    all_children_exited = false;
                    break;
                }
                child = child->next;
            }
            if (all_children_exited)
            {
                unblock_process(process);
            }
        }

        process = process->next;
    }
}

/**
 * @brief Run the next process
 *
 * This function runs the next process from the queue that was selected.
 *
 * @param scheduler_state The scheduler state
 */
void _run_next_process()
{
    // Update the blocked processes before selecting the next process
    _update_blocked_processes();

    // Select the next queue to run a process from
    int next_queue = _select_next_queue(scheduler_state);

    if (next_queue == -1)
    {
        // No process to run, so we don't consume a quantum
        return;
    }

    // Get the process to run from the queue
    pcb_t *process = linked_list_pop_head(&scheduler_state->ready_queues[next_queue]);
    
    if (!process) {
        // This should ideally not happen if _select_next_queue returned a valid index
        fprintf(stderr, "Scheduler Error: No process found in selected ready queue %d\n", next_queue);
        return; // Don't consume quantum
    }

    if (process->thread == NULL)
    {
        // This process has no thread associated? Major error.
        fprintf(stderr, "Scheduler Error: Process PID %d dequeued but has NULL thread! Discarding.\n", process->pid);
        // Clean up this invalid PCB? If we just free it, internal pointers might be bad.
        // Add it to zombie queue? For now, just discard and don't consume quantum.
        // TODO: Decide on proper cleanup for invalid PCB state here.
        free(process); // Simplistic cleanup, might leak argv etc.
        return;
    }

    // Set the current process to the process that was just run
    scheduler_state->current_process = process;

    // Run the process and block the scheduler until the next SIGALRM arrives (100ms later)
    spthread_continue(*process->thread);
    sigsuspend(&suspend_set);
    spthread_suspend(*process->thread);

    // Consume a quantum
    quantum++;

    // Add the process back to the queue
    linked_list_push_tail(&scheduler_state->ready_queues[next_queue], process);
}

/**
 * @brief Run the scheduler
 *
 * This function runs the scheduler.
 *
 * @param scheduler_state The scheduler state
 */
void run_scheduler()
{
    while (1)
    {
        _run_next_process();
    }
}

// ================================ Process Management API ================================

/**
 * @brief Internal helper to remove a process from its current active queue 
 *        (ready, blocked, or stopped) without calling its destructor.
 *
 * This function manually unlinks the process from the doubly linked list.
 * It does NOT handle removal from the zombie queue or the parent's children list.
 * It also handles the case where the process might be the currently executing 
 * process (which isn't strictly in a queue).
 *
 * @param process The PCB of the process to remove.
 * @return true if the process was found and removed from a ready, blocked, or stopped queue, false otherwise.
 */
static bool k_remove_from_active_queue(pcb_t *process) {
    if (!process || !scheduler_state) return false;

    // Try removing from ready queues
    for (int i = 0; i < 3; i++) {
        // Use manual removal logic since linked_list_remove uses destructor
        pcb_t* current = scheduler_state->ready_queues[i].head;
        while (current != NULL) {
            if (current == process) {
                if (current->prev) current->prev->next = current->next;
                else scheduler_state->ready_queues[i].head = current->next;
                if (current->next) current->next->prev = current->prev;
                else scheduler_state->ready_queues[i].tail = current->prev;
                process->prev = process->next = NULL;
                return true;
            }
            current = current->next;
        }
    }

    // Try removing from blocked queue
    pcb_t* current = scheduler_state->blocked_queue.head;
    while (current != NULL) {
         if (current == process) {
            if (current->prev) current->prev->next = current->next;
            else scheduler_state->blocked_queue.head = current->next;
            if (current->next) current->next->prev = current->prev;
            else scheduler_state->blocked_queue.tail = current->prev;
            process->prev = process->next = NULL;
            return true;
        }
        current = current->next;
    }
    
    // Try removing from stopped queue
    current = scheduler_state->stopped_queue.head;
     while (current != NULL) {
         if (current == process) {
            if (current->prev) current->prev->next = current->next;
            else scheduler_state->stopped_queue.head = current->next;
            if (current->next) current->next->prev = current->prev;
            else scheduler_state->stopped_queue.tail = current->prev;
            process->prev = process->next = NULL;
            return true;
        }
        current = current->next;
    }

    // Special case: Check if it's the current running process (shouldn't strictly be in a queue then)
    // This might indicate an issue elsewhere, but handle defensively.
    if (scheduler_state->current_process == process) {
         // If it's the current process, it's not in a queue to be removed from in this context.
         // Or should we handle this case? Maybe return true as it's "active"?
         // For block/kill/exit, the caller should handle the current_process case.
         return false; // Let's say it wasn't found *in a queue*.
    }

    return false; // Not found in any active queue
}

/**
 * @brief Internal helper to remove a process from the zombie queue 
 *        without calling its destructor.
 *
 * This function manually unlinks the process from the zombie queue's doubly linked list.
 *
 * @param process The PCB of the process to remove from the zombie queue.
 * @return true if the process was found and removed, false otherwise.
 */
static bool k_remove_from_zombie_queue(pcb_t *process) {
     if (!process || !scheduler_state) return false;
     // Manual removal from zombie queue
     pcb_t* current = scheduler_state->zombie_queue.head;
     while (current != NULL) {
         if (current == process) {
            if (current->prev) current->prev->next = current->next;
            else scheduler_state->zombie_queue.head = current->next;
            if (current->next) current->next->prev = current->prev;
            else scheduler_state->zombie_queue.tail = current->prev;
            process->prev = process->next = NULL;
            return true; // Found and removed
        }
        current = current->next;
    }
    return false; // Not found
}


/**
 * @brief Moves a process from its current active queue (or current_process state)
 *        to the blocked queue and sets its state to PROCESS_BLOCKED.
 *
 * If the process is the current running process, it's conceptually removed from the 
 * active set. Otherwise, it's removed from its ready/blocked/stopped queue.
 * 
 * @param process The process to block.
 * @return true if the process was found and moved to the blocked queue, false otherwise.
 */
bool k_block_process(pcb_t *process)
{
    if (!process || !scheduler_state) return false;
    
    // We need to handle the case where the process to block is the currently running one.
    bool removed = false;
    if (scheduler_state->current_process == process) {
        // It's the current process, not in a ready queue. 
        // Mark as removed conceptually, scheduler loop will handle not putting it back.
        removed = true; 
    } else {
        // Remove from whichever active queue it's in (ready/blocked/stopped)
        removed = k_remove_from_active_queue(process);
    }

    if (removed) {
    // Add the process to the blocked queue
    linked_list_push_tail(&scheduler_state->blocked_queue, process);
        process->state = PROCESS_BLOCKED; // Update state
        return true;
    }
    return false; // Not found or couldn't be removed
}

/**
 * @brief Moves a process from the blocked queue to the appropriate ready queue 
 *        based on its priority and sets its state to PROCESS_RUNNING.
 *
 * @param process The process to unblock.
 * @return true if the process was found in the blocked queue and moved, false otherwise.
 */
bool k_unblock_process(pcb_t *process)
{
    if (!process || !scheduler_state) return false;

    // Manual removal from blocked queue
     bool removed = false;
     pcb_t* current = scheduler_state->blocked_queue.head;
     while (current != NULL) {
         if (current == process) {
            if (current->prev) current->prev->next = current->next;
            else scheduler_state->blocked_queue.head = current->next;
            if (current->next) current->next->prev = current->prev;
            else scheduler_state->blocked_queue.tail = current->prev;
            process->prev = process->next = NULL;
            removed = true;
            break; 
        }
        current = current->next;
    }

    if (removed) {
        // Add the process to the appropriate ready queue based on its priority
        process->state = PROCESS_RUNNING; // Update state
        k_add_to_ready_queue(process); // Uses the existing function
        return true;
    }
    return false; // Not found in blocked queue
}


/**
 * @brief Finds a process by its PID across active scheduler queues (ready, blocked, stopped)
 *        and checks the currently running process. Does NOT search the zombie queue.
 *
 * @param pid The PID of the process to find.
 * @return pcb_t* Pointer to the PCB if found, NULL otherwise.
 */
pcb_t *k_get_process_by_pid(pid_t pid)
{
     if (!scheduler_state) return NULL;

     // Check current process first
     if (scheduler_state->current_process && scheduler_state->current_process->pid == pid) {
         return scheduler_state->current_process;
     }

    // Check ready queues
    for (int i = 0; i < 3; i++)
    {
        pcb_t *process = scheduler_state->ready_queues[i].head;
        while (process != NULL)
        {
            if (process->pid == pid)
            {
                return process;
            }
            process = process->next;
        }
    }
    // Check blocked queue
    pcb_t *process = scheduler_state->blocked_queue.head;
    while (process != NULL)
    {
        if (process->pid == pid)
        {
            return process;
        }
        process = process->next;
    }
    // Check stopped queue
    process = scheduler_state->stopped_queue.head;
    while (process != NULL)
    {
        if (process->pid == pid)
        {
            return process;
        }
        process = process->next;
    }
    // NOTE: Intentionally not checking zombie queue here. 
    // Use k_get_process_by_pid_including_zombies if needed.
    return NULL; 
}

/**
 * @brief Internal helper to find a specific child process by PID within a parent's children list.
 *
 * @param parent The parent process PCB.
 * @param pid The PID of the child to search for.
 * @return pcb_t* Pointer to the child PCB if found, NULL otherwise.
 */
static pcb_t* k_find_child_by_pid(pcb_t *parent, pid_t pid) {
    if (!parent || !parent->children) return NULL;
    pcb_t* child = parent->children->head;
    while(child) {
        if (child->pid == pid) {
            return child;
        }
        child = child->next;
    }
    return NULL;
}

/**
 * @brief Internal helper to find *any* zombie child within a parent's children list.
 *
 * @param parent The parent process PCB.
 * @return pcb_t* Pointer to the first zombie child PCB found, NULL if none exist.
 */
static pcb_t* k_find_zombie_child(pcb_t *parent) {
     if (!parent || !parent->children) return NULL;
    pcb_t* child = parent->children->head;
    while(child) {
        // Check state directly - this is internal kernel logic
        if (child->state == PROCESS_ZOMBIED) { 
            return child;
        }
        child = child->next;
    }
    return NULL;
}

/**
 * @brief Internal helper to remove a child from its parent's children list
 *        using manual unlinking. Does NOT call destructor or free PCB.
 *
 * @param parent The parent process PCB.
 * @param child The child process PCB to remove from the parent's list.
 * @return true if the child was successfully unlinked, false otherwise (e.g., NULL pointers).
 */
static bool k_remove_child_from_parent_list(pcb_t *parent, pcb_t *child) {
    if (!parent || !parent->children || !child) return false;

    linked_list(pcb_t)* list = parent->children;
    
    // Manual unlink
    if (child->prev) {
        child->prev->next = child->next;
    } else {
        list->head = child->next; // Child was head
    }
    if (child->next) {
        child->next->prev = child->prev;
    } else {
        list->tail = child->prev; // Child was tail
    }
    
    child->prev = NULL;
    child->next = NULL;
    // TODO: Decrement parent's children count if we track it.
    return true;
}


/**
 * @brief Internal helper to reap a specific zombie child.
 *
 * This function performs the final cleanup for a terminated child process:
 * 1. Collects the exit status.
 * 2. Joins the underlying spthread.
 * 3. Frees the spthread_t structure.
 * 4. Removes the child from its parent's children list.
 * 5. Removes the child from the global zombie queue.
 * 6. Calls k_proc_cleanup to free the PCB and its associated resources.
 *
 * @param child The PCB of the zombie child process to reap.
 * @param wstatus Pointer to an integer where the child's exit status will be stored (if not NULL).
 * @return pid_t The PID of the reaped child on success, -1 on error (e.g., child was not a zombie).
 */
static pid_t k_reap_child(pcb_t *child, int *wstatus) {
    if (!child || child->state != PROCESS_ZOMBIED) {
        return -1; // Not a zombie or NULL child
    }
    
    pid_t child_pid = child->pid;

    // 1. Get exit status
    if (wstatus) {
        *wstatus = child->exit_status;
    }

    // 2. Join the underlying thread (important!)
    //    This assumes the thread actually terminated. If not, this could block.
    //    k_proc_exit should ensure the thread function has returned before marking zombie.
    if (child->thread) {
         // We might not *need* the return value if exit status is already stored.
         spthread_join(*(child->thread), NULL); 
         // Free the thread struct itself now that it's joined
         free(child->thread);
         child->thread = NULL; 
    }


    // 3. Remove from parent's children list
    // Need the parent PCB. Get it using the PPID.
    // Note: Parent might have exited! If so, child should have been reparented to init.
    pcb_t *parent = k_get_process_by_pid(child->ppid); // Find parent (could be init)
    if (parent) {
        k_remove_child_from_parent_list(parent, child);
    } else {
        // Parent not found (potentially exited and cleaned up?). Log this?
        fprintf(stderr, "Kernel Warning: Parent PID %d not found when reaping child PID %d\n", child->ppid, child_pid);
    }


    // 4. Remove from the global zombie queue
    k_remove_from_zombie_queue(child);

    // 5. Perform final cleanup of the PCB and associated resources
    // k_proc_cleanup handles freeing argv, command, children list (which should be empty now), and the PCB itself.
    // It expects the child to be unlinked from lists already.
    k_proc_cleanup(child); // This frees the 'child' pointer.

    return child_pid;
}


/**
 * @brief Kernel-level implementation for waiting on a child process.
 *
 * Handles finding zombie children (either specific PID or any), reaping them using k_reap_child,
 * or blocking the parent if no appropriate zombie child is found and `nohang` is false.
 *
 * @param parent The PCB of the calling (parent) process.
 * @param pid The PID of the child to wait for (-1 for any child).
 * @param wstatus Pointer to store the exit status of the reaped child.
 * @param nohang If true, return immediately if no child has changed state.
 * @return pid_t PID of the reaped child on success.
 *         Returns 0 if `nohang` is true and no child was ready to be reaped.
 *         Returns -1 on error (e.g., no such child exists - ECHILD, invalid arguments).
 *         Returns -2 if blocking is required (and `nohang` is false) - Note: This indicates the 
 *              calling system call wrapper needs to handle yielding and retrying.
 */
pid_t k_waitpid(pcb_t *parent, pid_t pid, int *wstatus, bool nohang) {
    if (!parent || !scheduler_state) {
        return -1; // Invalid parent or scheduler state
    }

    pcb_t *child_to_reap = NULL;

    if (pid == -1) {
        // Wait for *any* child
        child_to_reap = k_find_zombie_child(parent);
        if (!child_to_reap) {
            // No zombie child found
             if (linked_list_is_empty(parent->children)) {
                 return -1; // No children exist (ECHILD)
             }
             if (nohang) {
                 return 0; // No zombie ready, non-blocking
             }
             // Block the parent - needs more sophisticated blocking mechanism
             // e.g., parent->waiting_for_child = ANY_CHILD; k_block_process(parent);
             // For now, we'll just block generically and rely on scheduler to wake it.
             // This isn't ideal, as it could be woken for other reasons.
             // TODO: Implement proper condition blocking for waitpid
             fprintf(stderr, "k_waitpid: Blocking for any child (PID %d) - requires proper blocking mechanism!\n", parent->pid);
             parent->state = PROCESS_BLOCKED; // Mark as blocked
             k_block_process(parent);          // Move to blocked queue
             
             // When unblocked (by scheduler eventually, or ideally by k_proc_exit), 
             // the syscall wrapper (s_waitpid) would likely need to retry the k_waitpid call.
             // Returning a specific code might be better, e.g., -EAGAIN. For now, return error.
             return -2; // Indicate blocking occurred (caller needs to handle retry)

        }
    } else if (pid > 0) {
        // Wait for a specific child PID
        pcb_t* child = k_find_child_by_pid(parent, pid);
        if (!child) {
            return -1; // No such child (ECHILD)
        }

        if (child->state == PROCESS_ZOMBIED) {
            child_to_reap = child;
        } else {
            // Child exists but is not a zombie
            if (nohang) {
                return 0; // Non-blocking, child not ready
            }
            // Block the parent - needs specific child blocking
            // e.g., parent->waiting_for_child = pid; k_block_process(parent);
            // TODO: Implement proper condition blocking for waitpid
             fprintf(stderr, "k_waitpid: Blocking for specific child PID %d (Parent PID %d) - requires proper blocking mechanism!\n", pid, parent->pid);
             parent->state = PROCESS_BLOCKED; // Mark as blocked
             k_block_process(parent);          // Move to blocked queue
             return -2; // Indicate blocking occurred (caller needs to handle retry)
        }
    } else {
        // pid <= 0 and pid != -1 (e.g., wait for process group) - Not implemented
        fprintf(stderr, "k_waitpid: Waiting for process group (pid <= 0, != -1) is not implemented.\n");
        return -1; // EINVAL or ENOSYS
    }

    // If we found a zombie child to reap
    if (child_to_reap) {
        return k_reap_child(child_to_reap, wstatus);
    }

    // Should not be reached if logic is correct, but return error just in case.
    return -1; 
}

/**
 * @brief Marks a process as terminated (zombie), sets its exit status,
 *        removes it from active queues, moves it to the zombie queue, 
 *        and potentially unblocks a waiting parent.
 *
 * This is the first stage of process termination. The actual cleanup (joining thread, 
 * freeing PCB) happens later when the process is reaped by its parent via k_waitpid/k_reap_child.
 * It assumes the process's thread function has already completed or is about to terminate.
 *
 * @param process The PCB of the process that is exiting.
 * @param exit_status The exit status code for the process.
 */
void k_proc_exit(pcb_t *process, int exit_status) {
     if (!process || !scheduler_state) return;

     // 1. Set state and exit status
     process->state = PROCESS_ZOMBIED;
     process->exit_status = exit_status;

     // 2. Remove from any active queue it might be in (ready/blocked/stopped/current)
     // If it's the current process, the scheduler loop needs to handle not running it again.
     if (scheduler_state->current_process == process) {
         // Mark it so the scheduler loop knows not to requeue it
         // Maybe set scheduler_state->current_process = NULL; here? Or handle in scheduler loop.
         // Let's assume scheduler loop checks state after sigsuspend.
     } else {
          k_remove_from_active_queue(process); // Remove from ready/blocked/stopped
     }
     
     // 3. Add to the zombie queue
     linked_list_push_tail(&scheduler_state->zombie_queue, process);


     // 4. Check if the parent is waiting and unblock it
     pcb_t *parent = k_get_process_by_pid(process->ppid);
     if (parent && parent->state == PROCESS_BLOCKED) {
         // TODO: Add more specific check if parent is blocked *specifically* for this child
         // e.g., if (parent->waiting_for_child == process->pid || parent->waiting_for_child == ANY_CHILD)
         // For now, unblock if parent is blocked for any reason and one of its children exited.
         fprintf(stdout, "k_proc_exit: Unblocking parent PID %d because child PID %d exited.\n", parent->pid, process->pid);
         k_unblock_process(parent); 
     }

     // NOTE: The actual thread *must* have exited its main function before this is called.
     // The spthread_join happens later during reaping (in k_reap_child).
     
     // If the exiting process *was* the current process, yield control back to scheduler.
     // This might involve signaling or simply letting the scheduler loop take over.
     // If called from s_exit, it won't return. If called implicitly (func returns),
     // the wrapper that called func should probably yield or exit.
     // For now, assume the context switch happens appropriately elsewhere.
}

/**
 * @brief Retrieves the Process Control Block (PCB) of the currently running process.
 *
 * @return pcb_t* Pointer to the current process's PCB, or NULL if the scheduler
 *         state is not initialized or no process is currently assigned.
 */
pcb_t* k_get_current_process(void) {
    if (!scheduler_state) {
        return NULL; // Scheduler not initialized
    }
    return scheduler_state->current_process;
}

/**
 * @brief Voluntarily yields the CPU to the scheduler.
 *
 * Allows the scheduler to run other processes. The calling process will be
 * paused and resumed later according to the scheduling policy.
 * Placeholder implementation: Suspends the calling thread until the next
 * scheduler timer interrupt (SIGALRM).
 */
void k_yield(void) {
    // In a real kernel, this would directly invoke the scheduler's context
    // switching logic.
    // Here, we mimic yielding by suspending until the next SIGALRM, 
    // which triggers the scheduler loop externally.
    extern sigset_t suspend_set; // Defined static in scheduler.c
    sigsuspend(&suspend_set);
    // Execution resumes here after SIGALRM is handled
}

/**
 * @brief Stops a process, moving it to the stopped queue.
 * Removes the process from active queues and sets its state.
 * @param process The process to stop.
 * @return true on success, false if process not found or already stopped.
 */
bool k_stop_process(pcb_t *process) {
    if (!process || process->state == PROCESS_STOPPED) {
        return false;
    }
    // Remove from active queue (or current)
    bool removed = false;
    if (scheduler_state->current_process == process) {
        removed = true; // Conceptually removed, scheduler handles switch
    } else {
        removed = k_remove_from_active_queue(process); 
    }

    if (removed) {
        process->state = PROCESS_STOPPED;
        linked_list_push_tail(&scheduler_state->stopped_queue, process);
        return true;
    }
    return false;
}

/**
 * @brief Continues a stopped process, moving it back to the ready queue.
 * Removes the process from the stopped queue and sets its state.
 * @param process The process to continue.
 * @return true on success, false if process not found or not stopped.
 */
bool k_continue_process(pcb_t *process) {
    if (!process || process->state != PROCESS_STOPPED) {
        return false;
    }

    // Manual removal from stopped queue
    bool removed = false;
    pcb_t* current = scheduler_state->stopped_queue.head;
    while (current != NULL) {
         if (current == process) {
            if (current->prev) current->prev->next = current->next;
            else scheduler_state->stopped_queue.head = current->next;
            if (current->next) current->next->prev = current->prev;
            else scheduler_state->stopped_queue.tail = current->prev;
            process->prev = process->next = NULL;
            removed = true;
            break; 
        }
        current = current->next;
    }

    if (removed) {
        process->state = PROCESS_RUNNING;
        k_add_to_ready_queue(process); // Add to appropriate ready queue
        return true;
    }
    return false;
}

/**
 * @brief Sets the priority of a process.
 * If the process is currently ready, it will be moved to the correct ready queue.
 * If blocked or stopped, only the priority field is updated.
 * @param process The process to modify.
 * @param priority The new priority (PRIORITY_HIGH, PRIORITY_MEDIUM, PRIORITY_LOW).
 * @return true on success, false if process not found or invalid priority.
 */
bool k_set_priority(pcb_t* process, int priority) {
    if (!process || priority < PRIORITY_HIGH || priority > PRIORITY_LOW) {
        return false;
    }

    int old_priority = process->priority;
    process->priority = (priority_t)priority;

    // Only move queues if the process is currently in a ready queue
    if (process->state == PROCESS_RUNNING && old_priority != priority) {
        // Need to find and remove from the *old* ready queue first
        bool removed = false;
        pcb_t* current = scheduler_state->ready_queues[old_priority].head;
        while (current != NULL) {
            if (current == process) {
                if (current->prev) current->prev->next = current->next;
                else scheduler_state->ready_queues[old_priority].head = current->next;
                if (current->next) current->next->prev = current->prev;
                else scheduler_state->ready_queues[old_priority].tail = current->prev;
                process->prev = process->next = NULL;
                removed = true;
                break;
            }
            current = current->next;
        }

        if (removed) {
            // Add to the new ready queue
            k_add_to_ready_queue(process); 
        } else {
            // Process was RUNNING but not found in its expected ready queue? Log error.
            fprintf(stderr, "k_set_priority Warning: Process PID %d state is RUNNING but not found in ready queue %d.\n", process->pid, old_priority);
            // Still update priority field, but return false as queue move failed.
            return false;
        }
    }
    // If process was blocked, stopped, or priority didn't change, 
    // just updating the field is sufficient.
    return true; 
}

/**
 * @brief Puts the calling process to sleep for a specified number of ticks.
 * The process is blocked, and sleep_time is set.
 * Caller should likely call k_yield() after this.
 * @param process The process to put to sleep.
 * @param ticks The number of ticks to sleep (must be > 0).
 * @return true on success, false if process is NULL or ticks is 0.
 */
bool k_sleep(pcb_t* process, unsigned int ticks) {
    if (!process || ticks == 0) {
        return false;
    }

    process->sleep_time = ticks; 
    // k_block_process handles removing from active queue and adding to blocked queue,
    // and sets state to PROCESS_BLOCKED.
    return k_block_process(process); 
}

void k_get_processes_from_queue(linked_list(pcb_t)* queue) {
    pcb_t* current = queue->head;
    while (current != NULL) {
        printf("PID: %d, PPID: %d, Priority: %d, State: %d\n", current->pid, current->ppid, current->priority, current->state);
        current = current->next;
    }
}

// get info for running, blocked, and stopped processes
void k_get_all_process_info() {
    k_get_processes_from_queue(&scheduler_state->ready_queues[PRIORITY_HIGH]);
    k_get_processes_from_queue(&scheduler_state->ready_queues[PRIORITY_MEDIUM]);
    k_get_processes_from_queue(&scheduler_state->ready_queues[PRIORITY_LOW]);
    k_get_processes_from_queue(&scheduler_state->blocked_queue);
    k_get_processes_from_queue(&scheduler_state->stopped_queue);
}
