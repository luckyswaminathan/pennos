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
    if (pid == -1) {
        // Wait for any child process
        pcb_t* child = scheduler_state->current_process->children->head;
        pcb_t* zombie_child = NULL;
        
        // First pass: look for zombies
        while (child != NULL) {
            pcb_t* next = child->next; // Save next pointer as we might remove child
            
            if (child->state == PROCESS_ZOMBIED) {
                // Found a zombie, collect its status
                zombie_child = child;
                if (wstatus != NULL) {
                    *wstatus = zombie_child->exit_status;
                }
                
                // Remove from zombie queue and children list, ele_dtor should handle freeing but TODO check
                linked_list_remove(scheduler_state->current_process->children, zombie_child);
                linked_list_remove(&scheduler_state->zombie_queue, zombie_child);
                
                pid_t result = zombie_child->pid;
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
        return s_waitpid(-1, wstatus, false);
    } else {
        // Wait for specific child
        pcb_t* child = get_process_by_pid(pid);
        
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
            linked_list_remove(scheduler_state->current_process->children, child);
            linked_list_remove(&scheduler_state->zombie_queue, child);
            
            pid_t result = child->pid;
            return result;
        }
        
        if (nohang) {
            // Child exists but isn't zombie, and nohang is true
            return 0;
        }
        
        // Need to wait for specific child to terminate
        block_and_wait(scheduler_state, scheduler_state->current_process, child, wstatus);
        
        // After child terminates, it should be a zombie
        // Return its PID after removing it from zombie queue and freeing
        linked_list_remove(scheduler_state->current_process->children, child);
        linked_list_remove(&scheduler_state->zombie_queue, child);
        
        pid_t result = child->pid;
        return result;
    }
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
