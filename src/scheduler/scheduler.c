#include <string.h>
#include <stdlib.h>
#include <stdarg.h> // Needed for va_list, etc.
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
#include "src/utils/error_codes.h"

// TODO: remove this as soon as we switch k_log
#include <stdio.h>

scheduler_t *scheduler_state = NULL;
static const int centisecond = 10000;
static int quantum = 0;
static sigset_t suspend_set;
static bool extra_logging_enabled = false; // Added global toggle

/**
 * @brief The process to run at each quantum. Randomly disperesed array of 0s, 1s, and 2s (priority levels).
 *
 * This array is used to test the scheduler by running a specific process at each quantum.
 * We have nine 0s, six 1s, and four 2s.
 *
 */
static int process_to_run[19] = {0, 0, 1, 0, 0, 1, 2, 0, 1, 1, 0, 0, 1, 2, 0, 2, 1, 0, 2};

/**
 * @brief General kernel logger function.
 * Prints messages to stderr only if extra_logging_enabled is true.
 * Uses printf-style formatting.
 * @param format The format string.
 * @param ... Variable arguments for the format string.
 */
void k_log(const char *format, ...) {
    if (!extra_logging_enabled) {
        return;
    }
    va_list args;
    va_start(args, format);
    // TODO: swap with k_write (currently have k_fprintf_short which can only handle 1023 characters)
    vfprintf(stderr, format, args);
    va_end(args);
}

/**
 * @brief PCB destructor function for linked lists
 *
 * This function is used as a destructor for PCBs in linked lists.
 * It frees the memory allocated for the PCB and its associated resources.
 *
 * @param pcb Pointer to the PCB to destroy
 */
// void pcb_destructor(void *pcb)
// {
//     pcb_t *pcb_ptr = (pcb_t *)pcb;
//     if (pcb_ptr != NULL)
//     {
//         // Free the children list if it exists
//         if (pcb_ptr->children != NULL)
//         {
//             linked_list_clear(pcb_ptr->children);
//             free(pcb_ptr->children);
//         }

//         // Free the thread if it exists
//         if (pcb_ptr->thread != NULL)
//         {
//             // Properly join the thread to clean up its resources
//             spthread_join(*pcb_ptr->thread, NULL);
//             free(pcb_ptr->thread);
//         }

//         // Free command and argv if they exist
//         if (pcb_ptr->command != NULL)
//         {
//             free(pcb_ptr->command);
//         }

//         if (pcb_ptr->argv != NULL)
//         {
//             for (int i = 0; pcb_ptr->argv[i] != NULL; i++)
//             {
//                 free(pcb_ptr->argv[i]);
//             }
//             free(pcb_ptr->argv);
//         }
//         // Free the PCB itself
//         free(pcb_ptr);
//     }
// }

void pcb_destructor(void* pcb) {}

/**
 * @brief Signal handler for SIGALRM
 *
 * This function is used to handle the SIGALRM signal.
 * It is used to trigger the scheduler by sending SIGALRM every 100ms
 */
static void alarm_handler(int signum) {
}

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

    scheduler_state->terminal_controlling_pid = 0; // No foreground process by default

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
int k_add_to_ready_queue(pcb_t *process)
{
    if (!scheduler_state) return E_INVALID_SCHEDULER_STATE; // Basic safety check
    if (!process) return E_INVALID_PCB; // Basic safety check

    // Ensure priority is within valid range (optional, but good practice)
    if (process->priority < PRIORITY_HIGH || process->priority > PRIORITY_LOW) {
        // Handle error - log? Default to medium? For now, just return.
        k_fprintf_short(STDERR_FILENO, "Kernel Error: Process PID %d has invalid priority %d\n", process->pid, process->priority);
        return E_INVALID_PCB;
    }
    
    // Use the linked list macro to add to the tail of the correct priority queue
    k_log("Adding process PID %d to priority queue %d\n", process->pid, process->priority);
    linked_list_push_tail(&scheduler_state->ready_queues[process->priority], process);
    process->state = PROCESS_RUNNING; // Ensure state reflects it's ready
    return 0;
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
           
            process->sleep_time -= 0.1;
            if (process->sleep_time <= 0)
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
    pcb_t *process = linked_list_head(&scheduler_state->ready_queues[next_queue]);

    
    if (!process) {
        // This should ideally not happen if _select_next_queue returned a valid index
        return; // Don't consume quantum
    }

    if (process->thread == NULL)
    {
        // This process has no thread associated? Major error.
        k_fprintf_short(STDERR_FILENO, "Scheduler Error: Process PID %d dequeued but has NULL thread! Discarding.\n", process->pid);
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
    if (process->state == PROCESS_ZOMBIED) {
        
        pcb_t* blocked_ptr = linked_list_head(&scheduler_state->blocked_queue);
        while (blocked_ptr != NULL) {
            k_log("BLOCKED PTR PID %d\n", blocked_ptr->pid);
            if (blocked_ptr->waited_child == process->pid) {
                //TODO: might need to refactor logic
                k_log("unblocking process IN RUN NEXT PROCESS %d\n", blocked_ptr->pid);
                unblock_process(blocked_ptr);
                if (k_tcgetpid() == process->pid) {
                    k_tcsetpid(blocked_ptr->pid);
                }
                k_get_all_process_info();
                break;
            } else if (blocked_ptr->waited_child == -1) {
                //
            }
            blocked_ptr = blocked_ptr->next;
            // TODO: add -1 case
        }

        child_process_t* children_ptr = linked_list_head(process->children);
        while (children_ptr != NULL) {
            children_ptr->process->ppid = 1;
            child_process_t* next_children_ptr = children_ptr->next;
            linked_list_push_tail(scheduler_state->init_process->children, children_ptr);
            children_ptr = next_children_ptr;
        }
    } else if (linked_list_head(&scheduler_state->ready_queues[next_queue]) != NULL){
        pcb_t* head = linked_list_pop_head(&scheduler_state->ready_queues[next_queue]); // remove from queue
        linked_list_push_tail(&scheduler_state->ready_queues[next_queue], head);
    }
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
 * @brief Block a process
 *
 * This function blocks a process by removing it from the queue it is currently on and adding it to the blocked queue.
 *
 * @param process The process to block
 */
void block_process(pcb_t *process)
{
    // Remove the process from the queue it is currently on
    k_log("Blocking process with pid %d\n", process->pid);
    //k_get_all_process_info();
    linked_list_remove(&scheduler_state->ready_queues[process->priority], process);

    process->state = PROCESS_BLOCKED;
    // Add the process to the blocked queue
    linked_list_push_tail(&scheduler_state->blocked_queue, process);
    k_log("Post push tail\n");

    pcb_t* curr = linked_list_head(&scheduler_state->blocked_queue);
    while (curr != NULL) {
        k_log("Blocked process PID %d\n", curr->pid);
        curr = curr->next;
    }
}

/**
 * @brief Unblock a process
 *
 * This function unblocks a process by removing it from the blocked queue and adding it to the appropriate queue based on its priority.
 *
 * @param process The process to unblock
 */
void unblock_process(pcb_t *process)
{
    // Remove the process from the blocked queue
    linked_list_remove(&scheduler_state->blocked_queue, process);
    process->state = PROCESS_RUNNING;
    // Add the process to the appropriate queue based on its priority
    linked_list_push_tail(&scheduler_state->ready_queues[process->priority], process);
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
 * @brief Block and wait for a child process to finish
 *
 * This function blocks the parent process and waits for the child process to finish.
 *
 * @param scheduler_state The scheduler state
 * @param process The parent process
 * @param child The child process
 * @param wstatus The status of the child process
 */
void block_and_wait(scheduler_t *scheduler_state, pcb_t *process, pcb_t *child, int *wstatus) {
    k_log("Blocking and waiting for child with pid %d\n", child->pid);

    // this is unintuitive but works for blocking and waiting
    block_process(scheduler_state->current_process);
    spthread_suspend_self();
    k_log("Blocked\n");
    k_log("Status %d\n", *wstatus);

    // Wait for the child to finish
    k_log("Child finished\n");
    k_get_all_process_info();

    linked_list_remove(&scheduler_state->zombie_queue, child);

    k_get_all_process_info();
    // Mark child as zombie and add to zombie queue

    // TODO: actually cleanup/free the child

    *wstatus = child->exit_status;
    // Unblock the parent process
    // unblock_process(scheduler_state->current_process);
}

/**
 * @brief Remove a child from the children list of a process
 * 
 * This function searches through the children list of a process and removes
 * a child process from it. It compares the process pointer of each child in
 * the list to find the child process to remove.
 * 
 * @param process The process to remove the child from
 * @param child The child process to remove
 */
void remove_from_children_list(pcb_t *process, pcb_t *child) {
    for (child_process_t* child_process = process->children->head; child_process != NULL; child_process = child_process->next) {
        if (child_process->process == child) {
            linked_list_remove(process->children, child_process);
            return;
        }
    }
}


/**
 * @brief Wait on a child of the calling process, until it changes state.
 * If `nohang` is true, this will not block the calling process and return immediately.
 * 
 * First clean up zombies, then check nohang status, then block and wait if required.
 *
 * @param pid Process ID of the child to wait for.
 * @param wstatus Pointer to an integer variable where the status will be stored.
 * @param nohang If true, return immediately if no child has exited.
 * @return pid_t The process ID of the child which has changed state on success, -1 on error.
 */
pid_t k_waitpid(pid_t pid, int* wstatus, bool nohang) {
    if (pid == -1) {
        // Wait for any child process
        scheduler_state->current_process->waited_child = -1;
        child_process_t* child = scheduler_state->current_process->children->head;
        child_process_t* zombie_child = NULL;
        
        // First pass: look for zombies
        while (child != NULL) {
            child_process_t* next = child->next; // Save next pointer as we might remove child
            
            if (child->process->state == PROCESS_ZOMBIED) {
                // Found a zombie, collect its status
                zombie_child = child;
                if (wstatus != NULL) {
                    *wstatus = zombie_child->process->exit_status;
                }
                
                // Remove from zombie queue and children list, ele_dtor should handle freeing but TODO check
                linked_list_remove(scheduler_state->current_process->children, zombie_child);
                linked_list_remove(&scheduler_state->zombie_queue, zombie_child->process);
                
                pid_t result = zombie_child->process->pid;
                return result;
            }
            
            child = next;
        }
        
        // No zombies found
        if (scheduler_state->current_process->children->head == NULL) {
            // No children at all
            return -1;
        }
        
        if (nohang) {
            // Have children, but none are zombies and nohang is true
            return 0;
        }
        
        // Need to wait for any child to terminate
        // Block parent until a child terminates
        block_process(scheduler_state->current_process);
        
        // Parent will be unblocked when a child terminates and becomes zombie
        // After unblocking, recursively call waitpid to find and reap the zombie
        return k_waitpid(-1, wstatus, false);
    } else {
        k_log("Waiting for specific child with pid %d\n", pid);
        // Wait for specific child
        pcb_t* child = k_get_process_by_pid(pid);
        scheduler_state->current_process->waited_child = pid;
        k_log("Child found with pid %d\n", child->pid);
        
        if (child == NULL) {
            return -1; // No such process
        }
        
        // If the process is not a child of the calling process, return -1
        if (child->ppid != scheduler_state->current_process->pid) {
            return -1;
        }
        
        if (child->state == PROCESS_ZOMBIED) {
            // Process is zombie, collect its status
            if (wstatus != NULL) {
                *wstatus = child->exit_status;
            }
            
            // Remove from zombie queue and children list
            remove_from_children_list(scheduler_state->current_process, child);
            linked_list_remove(&scheduler_state->zombie_queue, child);
            
            pid_t result = child->pid;
            return result;
        }
        
        if (nohang) {
            // Child exists but isn't zombie, and nohang is true
            return 0;
        }
        
        // Need to wait for specific child to terminate

        k_log("current process pid: %d\n", scheduler_state->current_process->pid);
        k_log("waiting for %lu for %s\n", child->thread->thread, child->command);
        block_and_wait(scheduler_state, scheduler_state->current_process, child, wstatus);
        k_log("Unblocked and waiting for child with pid %d to terminate\n", child->pid);
        k_get_all_process_info();
        
        // After child terminates, it should be a zombie
        // Return its PID after removing it from zombie queue and freeing
        remove_from_children_list(scheduler_state->current_process, child);
        linked_list_remove(&scheduler_state->zombie_queue, child);
        
        pid_t result = child->pid;
        return result;
    }
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
 * @return 0 on success, and a negative error code on failure.
 */
int k_proc_exit(pcb_t *process, int exit_status) {
    if (!process) return E_INVALID_ARGUMENT;
    if (!scheduler_state) return E_INVALID_SCHEDULER_STATE;

     // 1. Set state and exit status
     process->state = PROCESS_ZOMBIED;
     process->exit_status = exit_status;
   

     // 2. Remove from any active queue it might be in (ready/blocked/stopped/current)
     // If it's the current process, the scheduler loop needs to handle not running it again.
     k_remove_from_active_queue(process); // Remove from ready/blocked/stopped
     // 3. Add to the zombie queue
     linked_list_push_tail(&scheduler_state->zombie_queue, process);

     // NOTE: The actual thread *must* have exited its main function before this is called.
     // The spthread_join happens later during reaping (in k_reap_child).
     
     // If the exiting process *was* the current process, yield control back to scheduler.
     // This might involve signaling or simply letting the scheduler loop take over.
     // If called from s_exit, it won't return. If called implicitly (func returns),
     // the wrapper that called func should probably yield or exit.
     // For now, assume the context switch happens appropriately elsewhere.


    // Ensure the thread terminates and doesn't return from s_exit.
    // Using spthread_exit is appropriate here if available and intended.
    // Alternatively, an infinite loop prevents return, relying on the scheduler 
    // to never schedule this zombie process again.

    // todo - when we call s_kill, the process is a bit weird
    if (process->pid == scheduler_state->current_process->pid) {
        spthread_exit(NULL); // Use spthread library's exit mechanism
    }

    return 0;
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
            int status = k_add_to_ready_queue(process); 
            if (status < 0) {
                return status;
            }
        } else {
            // Process was RUNNING but not found in its expected ready queue? Log error.
            k_fprintf_short(STDERR_FILENO, "k_set_p)riority Warning: Process PID %d state is RUNNING but not found in ready queue %d.\n", process->pid, old_priority);
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
    block_process(process); 
    //k_proc_exit(process, 0);
    return true;
}

void k_get_processes_from_queue(pcb_ll_t queue) {
    pcb_t* current = queue->head;
    while (current != NULL) {
        k_log("PID: %d, PPID: %d, Priority: %d, State: %d, Args: %s\n", current->pid, current->ppid, current->priority, current->state, current->argv[0]);
        current = current->next;
    }
}

/**
 * @brief Helper function to print processes from a queue in ps format.
 *
 * @param queue The queue to iterate over.
 * @param state_char The character representing the process state ('R', 'B', 'S', 'Z').
 */
int k_print_processes_from_queue(pcb_ll_t queue, char state_char) {
    pcb_t* current = queue->head;
    while (current != NULL) {
        // Don't print the currently executing process here, it's handled separately
        if (current != scheduler_state->current_process) { 
            // Adjusted format string for alignment, assuming reasonable PID/PPID width
            // assume the length of the command string is at most 100 characters
            int status = k_fprintf_short(
                STDERR_FILENO,
                "%3d %4d %3d %c    %s\n",
                current->pid, 
                current->ppid, 
                current->priority, 
                state_char, 
                current->command ? current->command : "<?>" // Use command name
            );
            if (status < 0) {
                return status;
            }
        }
        current = current->next;
    }
    return 0;
}

/**
 * @brief List all processes on PennOS, displaying PID, PPID, priority, status, 
 * and command name, similar to `ps`.
 *
 * Prints only if extra logging i s enabled via k_toggle_logging().
 * Status codes: R (Running), B (Blocked), S (Stopped), Z (Zombie).
 */
void k_get_all_process_info() {
    if (!extra_logging_enabled || !scheduler_state) {
        return;
    }

    // Print header once
    char* header = "PID PPID PRI STAT CMD\n";
    k_write(STDERR_FILENO, header, strlen(header));

    // Print current running process first (if any)
    pcb_t *current_proc = scheduler_state->current_process;
    if (current_proc && current_proc->state != PROCESS_ZOMBIED) { // Don't list as R if exiting
        k_fprintf_short(
            STDERR_FILENO,
            "%3d %4d %3d %c    %s\n", 
            current_proc->pid, 
            current_proc->ppid, 
            current_proc->priority, 
            'R', // Always 'R' for the current process
            current_proc->command ? current_proc->command : "<?>"
        );
    }


    // Print ready queues (excluding current)
    k_print_processes_from_queue((pcb_ll_t)&scheduler_state->ready_queues[PRIORITY_HIGH], 'R');
    k_print_processes_from_queue((pcb_ll_t)&scheduler_state->ready_queues[PRIORITY_MEDIUM], 'R');
    k_print_processes_from_queue((pcb_ll_t)&scheduler_state->ready_queues[PRIORITY_LOW], 'R');
    
    // Print blocked queue
    k_print_processes_from_queue((pcb_ll_t)&scheduler_state->blocked_queue, 'B');
    
    // Print stopped queue
    k_print_processes_from_queue((pcb_ll_t)&scheduler_state->stopped_queue, 'S');
    
    // Print zombie queue
    k_print_processes_from_queue((pcb_ll_t)&scheduler_state->zombie_queue, 'Z');
}

/**
 * @brief Toggles the extra logging feature on or off.
 */
void k_toggle_logging() {
    extra_logging_enabled = !extra_logging_enabled;
    k_fprintf_short(STDERR_FILENO, "Extra logging %s.\n", extra_logging_enabled ? "enabled" : "disabled");
}

void k_tcsetpid(pid_t pid) {
    scheduler_state->terminal_controlling_pid = pid;
}

pid_t k_tcgetpid() {
    return scheduler_state->terminal_controlling_pid;
}
