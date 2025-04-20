#include "scheduler.h"
#include "kernel.h"
#include "logger.h"
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
pid_t s_spawn(void* (*func)(void*), char *argv[], int fd0, int fd1) {
    // Get the parent process PCB using the kernel helper function.
    // This avoids direct access to scheduler_state from the system call layer.
    pcb_t* parent = k_get_current_process();

    // Directly call the kernel function to create the process.
    // k_proc_create now handles PCB setup, thread creation, and scheduling.
    pid_t new_pid = k_proc_create(parent, func, argv, fd0, fd1);

    // k_proc_create returns -1 on error, so we can return that directly.
    if (new_pid < 0) {
        // Optionally log the error at the syscall level if desired
        fprintf(stderr, "s_spawn: Failed to create process.\n");
    }

    return new_pid;
}


/**
 * @brief Wait on a child of the calling process, until it changes state (zombies).
 * If `nohang` is true, this will not block the calling process and return immediately.
 * 
 * First clean up zombies, then check nohang status, then block and wait if required.
 *
 * @param pid Process ID of the child to wait for (-1 for any child).
 * @param wstatus Pointer to an integer variable where the exit status will be stored.
 * @param nohang If true, return immediately if no child has zombied.
 * @return pid_t The process ID of the zombied child on success, 0 if nohang and no child zombied, -1 on error.
 */
pid_t s_waitpid(pid_t pid, int* wstatus, bool nohang) {
    // 1. Get the current process PCB (the parent)
    pcb_t* parent = k_get_current_process();
    if (!parent) {
        // Should not happen if called from a running process, but handle defensively.
        fprintf(stderr, "s_waitpid Error: Could not get current process.\n");
        return -1; // Or set appropriate errno
    }

    // 2. Call the kernel implementation
    // k_waitpid handles finding children, checking state, reaping, and blocking.
    pid_t result = k_waitpid(parent, pid, wstatus, nohang);

    // 3. Handle potential blocking case (if k_waitpid returns -2)
    // This requires a loop and potentially yielding/rescheduling, which is complex.
    // For now, we treat the blocking indicator as an error.
    // A more complete implementation would involve yielding and retrying.
    if (result == -2) { // Special return code from our current k_waitpid indicating blocking
        fprintf(stderr, "s_waitpid: Kernel indicated blocking; syscall needs retry mechanism (not implemented).\n");
        // In a real system, this might yield and retry, or return an error like EINTR/EAGAIN.
        // Set errno to EAGAIN perhaps? For now, map to -1.
        // errno = EAGAIN;
        return -1; 
    }
    
    // Return the result from k_waitpid (-1 for error, 0 for nohang, PID for success)
    return result; 
}

/**
 * @brief Send a signal to a particular process.
 *
 * @param pid Process ID of the target proces.
 * @param signal Signal number to be sent.
 * @return 0 on success, -1 on error.
 */
int s_kill(pid_t pid, int signal) {
    
}

/**
 * @brief Unconditionally exit the calling process.
 */
void s_exit(void);

/**
 * @brief Set the priority of the specified thread.
 *
 * @param pid Process ID of the target thread.
 * @param priority The new priorty value of the thread (0, 1, or 2)
 * @return 0 on success, -1 on failure.
 */
int s_nice(pid_t pid, int priority);

/**
 * @brief Suspends execution of the calling proces for a specified number of clock ticks.
 *
 * This function is analogous to `sleep(3)` in Linux, with the behavior that the system
 * clock continues to tick even if the call is interrupted.
 * The sleep can be interrupted by a P_SIGTERM signal, after which the function will
 * return prematurely.
 *
 * @param ticks Duration of the sleep in system clock ticks. Must be greater than 0.
 */
void s_sleep(unsigned int ticks);
