#include "scheduler.h"

/**
 * @brief Create a new child process.
 * @param parent The parent process PCB (can be NULL for initial processes).
 * @param func The function the new process should execute.
 * @param argv Null-terminated argument vector for the new process. The kernel will copy this.
 * @param fd0 Input file descriptor.
 * @param fd1 Output file descriptor.
 * @return pid_t The PID of the newly created process, or -1 on error.
 */
pid_t k_proc_create(pcb_t *parent, void *(*func)(void *), char *const argv[], int fd0, int fd1);

/**
 * @brief Clean up a terminated/finished process's resources.
 * Called internally by the kernel, typically after reaping.
 * @param proc Pointer to the PCB of the process to clean up.
 */
void k_proc_cleanup(pcb_t *proc);

/**
 * @brief Adds a process to the appropriate scheduler ready queue based on its priority.
 * @param proc The process control block to add.
 */
void k_add_to_ready_queue(pcb_t *proc);

/**
 * @brief Retrieves the Process Control Block (PCB) of the currently running process.
 * @return pcb_t* Pointer to the current process's PCB, or NULL if no process is running.
 */
pcb_t* k_get_current_process(void);

/**
 * @brief Finds a process by its PID across all scheduler queues (except zombie).
 * @param pid The PID of the process to find.
 * @return pcb_t* Pointer to the PCB if found, NULL otherwise.
 */
pcb_t* k_get_process_by_pid(pid_t pid);

/**
 * @brief Moves a process from its current ready/running queue to the blocked queue.
 * Does not change the process state field directly, assumes caller manages state.
 * @param process The process to block.
 * @return true if the process was found and moved, false otherwise.
 */
bool k_block_process(pcb_t *process);

/**
 * @brief Moves a process from the blocked queue to the appropriate ready queue.
 * Does not change the process state field directly, assumes caller manages state.
 * @param process The process to unblock.
 * @return true if the process was found and moved, false otherwise.
 */
bool k_unblock_process(pcb_t *process);



/**
 * @brief Marks a process as terminated (zombie), sets its exit status, 
 *        moves it to the zombie queue, and potentially unblocks a waiting parent.
 * @param process The process that is exiting.
 * @param exit_status The exit status code.
 */
void k_proc_exit(pcb_t *process, int exit_status);

/**
 * @brief Voluntarily yields the CPU to the scheduler.
 *
 * Allows the scheduler to run other processes. The calling process will be 
 * paused and resumed later according to the scheduling policy.
 * This is a placeholder implementation relying on signal suspension.
 */
void k_yield(void);

/**
 * @brief Stops a process, moving it to the stopped queue.
 * @param process The process to stop.
 * @return true on success, false if process not found or already stopped.
 */
bool k_stop_process(pcb_t *process);

/**
 * @brief Continues a stopped process, moving it back to the ready queue.
 * @param process The process to continue.
 * @return true on success, false if process not found or not stopped.
 */
bool k_continue_process(pcb_t *process);

/**
 * @brief Sets the priority of a process.
 * If the process is currently ready, it will be moved to the correct ready queue.
 * If blocked or stopped, only the priority field is updated.
 * @param process The process to modify.
 * @param priority The new priority (PRIORITY_HIGH, PRIORITY_MEDIUM, PRIORITY_LOW).
 * @return true on success, false if process not found or invalid priority.
 */
bool k_set_priority(pcb_t* process, int priority);

/**
 * @brief Puts the calling process to sleep for a specified number of ticks.
 * The process is blocked, and sleep_time is set.
 * @param process The process to put to sleep.
 * @param ticks The number of ticks to sleep (must be > 0).
 * @return true on success, false if process is NULL or ticks is 0.
 */
bool k_sleep(pcb_t* process, unsigned int ticks);
