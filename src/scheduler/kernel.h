#include "scheduler.h"

/**
 * @brief Create a new child process, inheriting applicable properties from the parent.
 * This function handles PCB allocation, thread creation, and adding the process
 * to the ready queue.
 *
 * @param parent The parent process PCB (can be NULL for initial processes).
 * @param func The function the new process should execute.
 * @param argv Null-terminated argument vector for the new process. The kernel will copy this.
 * @param fd0 Input file descriptor.
 * @param fd1 Output file descriptor.
 * @return pid_t The PID of the newly created process, or -1 on error.
 */
pid_t k_proc_create(pcb_t *parent, void *(*func)(void *), char *const argv[], int fd0, int fd1);

/**
 * @brief Clean up a terminated/finished thread's resources.
 * This may include freeing the PCB, handling children, etc.
 */
void k_proc_cleanup(pcb_t *proc);

/**
 * @brief Adds a process to the appropriate scheduler ready queue based on its priority.
 *
 * @param proc The process control block to add.
 */
void k_add_to_ready_queue(pcb_t *proc);

/**
 * @brief Retrieves the Process Control Block (PCB) of the currently running process.
 *
 * @return pcb_t* Pointer to the current process's PCB, or NULL if no process is running.
 */
pcb_t* k_get_current_process(void);
