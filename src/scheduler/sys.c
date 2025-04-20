#include "scheduler.h"
#include "kernel.h"
#include "logger.h"
#include "../../lib/exiting_alloc.h"
#include "../../lib/linked_list.h"
#include "spthread.h"

/**
 * @brief Create a child process that executes the function `func`.
 * The child will retain some attributes of the parent.
 *
 * @param func Function to be executed by the child process.
 * @param argv Null-terminated array of args, including the command name as argv[0].
 * @param fd0 Input file descriptor.
 * @param fd1 Output file descriptor.
 * @return pid_t The process ID of the created child process.
 */
pid_t s_spawn(void* (*func)(void*), char *argv[], int fd0, int fd1) {
    // Create a new process
    pcb_t* proc = k_proc_create(scheduler_state->init_process);

    // Create a new thread
    proc->thread = (spthread_t*) exiting_malloc(sizeof(spthread_t));
    if (spthread_create(proc->thread, NULL, func, argv) != 0) {
        // Add logging here
        return -1;
    }

    // Set the process's function, arguments, and file descriptors
    proc->pid = scheduler_state->process_count++;
    proc->ppid = scheduler_state->init_process->pid;
    proc->pgid = proc->pid;
    proc->children = linked_list_create();
    proc->func = func;
    proc->argv = argv;
    proc->fd0 = fd0;
    proc->fd1 = fd1;
    proc->priority = PRIORITY_MEDIUM;

    // Add the process to the medium priority queue
    linked_list_push_head(&scheduler_state->ready_queues[proc->priority], proc);

    return proc->pid;
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
int s_kill(pid_t pid, int signal);

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
