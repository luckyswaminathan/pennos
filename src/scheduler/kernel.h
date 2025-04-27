#include "scheduler.h"
#include "src/pennfat/fat.h" // TODO: this exposes mount and unmount to the kernel (unsure if that's OK)
#include "src/pennfat/fat_constants.h"


// TODO: add header guards across all header files

#define P_SIGTERM 1
#define P_SIGSTOP 2
#define P_SIGCONT 3

/**
 * @brief Create a new child process.
 * @param parent The parent process PCB (can be NULL for initial processes).
 * @param func The function the new process should execute.
 * @param argv Null-terminated argument vector for the new process. The kernel will copy this.
 * @return pid_t The PID of the newly created process, or an error code, which is a negative integer.
 */
pid_t k_proc_create(pcb_t *parent, void *(*func)(void *), char *const argv[], priority_t priority);

/**
 * @brief Clean up a terminated/finished process's resources.
 * Called internally by the kernel, typically after reaping.
 * @param proc Pointer to the PCB of the process to clean up.
 * @return 0 on success, or an error code, which is a negative integer.
 */
int k_proc_cleanup(pcb_t *proc);

/**
 * @brief Adds a process to the appropriate scheduler ready queue based on its priority.
 * @param proc The process control block to add.
 * @return 0 on success, or an error code, which is a negative integer.
 */
int k_add_to_ready_queue(pcb_t *proc);

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
 * @return 0 if successful, and a negative error code on failure.
 */
int k_proc_exit(pcb_t *process, int exit_status);

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

/**
 * @brief Sets a flag to indicate that the logout command has been issued
 * and the scheduler should exit. Since this only signals the scheduler to exit,
 * it is not guaranteed to exit immediately.
 */
void k_logout();
