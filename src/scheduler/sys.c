#include "src/scheduler/sys.h"
#include "scheduler.h"
#include "kernel.h"
#include "logger.h"
#include "../../lib/exiting_alloc.h"
#include "../../lib/linked_list.h"
#include "spthread.h"
#include <stdio.h>
#include <stdlib.h> // For NULL
#include <string.h> // For strcmp etc. if needed
#include <signal.h> // For signal definitions (SIGTERM, SIGSTOP, SIGCONT)
#include <unistd.h> // For _exit? No, use spthread_exit or infinite loop.
#include "src/utils/error_codes.h"
// Forward declaration for the scheduler function (to be implemented later)
void run_scheduler(); 

/**
 * @brief Create a child process that executes the function `func`.
 * The child will retain some attributes of the parent.
 *
 * @param func Function to be executed by the child process.
 * @param arg Argument to be passed to the function.
 * @return pid_t The process ID of the created child process, or -1 on error.
 */
pid_t s_spawn(void* (*func)(void*), char *argv[], int fd0, int fd1, priority_t priority) {
    // Get the parent process PCB using the kernel helper function.
    // This avoids direct access to scheduler_state from the system call layer.
    pcb_t* parent = k_get_current_process();

    // Directly call the kernel function to create the process.
    // k_proc_create now handles PCB setup, thread creation, and scheduling.
    pid_t new_pid = k_proc_create(parent, func, argv, priority);

    // k_proc_create returns -1 on error, so we can return that directly.
    if (new_pid < 0) {
        s_set_errno(new_pid);
        return -1;
    }
    
    if (parent != NULL) {
        if (fd0 < 0 || fd0 >= PROCESS_FD_TABLE_SIZE || fd1 < 0 || fd1 >= PROCESS_FD_TABLE_SIZE) {
            s_set_errno(E_INVALID_ARGUMENT);
            return -1;
        }
        process_fd_entry stdin_fd_entry = parent->process_fd_table[fd0];
        process_fd_entry stdout_fd_entry = parent->process_fd_table[fd1];
        if (stdin_fd_entry.in_use == false || stdout_fd_entry.in_use == false) {
            s_set_errno(E_INVALID_ARGUMENT);
            return -1;
        }

        // TODO: is this the right behavior? or is the intended behavior that the child process should inherit only the
        // *global* file descriptor that fd0 and fd1 (process level fds) point to?
        // The current implementation means that the child process will also inherit the mode and offset of the parent process
        k_get_process_by_pid(new_pid)->process_fd_table[STDIN_FD] = stdin_fd_entry;
        k_get_process_by_pid(new_pid)->process_fd_table[STDOUT_FD] = stdout_fd_entry; 
    }

    return new_pid;
}

bool P_WIFEXITED(int wstatus) {
    return (wstatus & 1);
}

bool P_WIFSTOPPED(int wstatus) {
    return (wstatus & 2);
}

bool P_WIFSIGNALED(int wstatus) {
    return (wstatus & 4);
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
    if (pid != -1 && !nohang) {
        s_tcsetpid(pid); // try to pass the terminal control to the child process we will be blocked on
    }
    int status = k_waitpid(pid, wstatus, nohang);
    if (status < 0) {
        s_set_errno(status);
        return -1;
    }
    return 0;
}

/**
 * @brief Send a signal to a particular process.
 * Current implementation supports SIGTERM, SIGSTOP, SIGCONT.
 *
 * @param pid Process ID of the target proces.
 * @param signal Signal number to be sent.
 * @return 0 on success, -1 on error (e.g., process not found, invalid signal).
 */
int s_kill(pid_t pid, int signal) {
    // Find the target process using the kernel helper
    pcb_t* target = k_get_process_by_pid(pid);
    if (!target) {
        s_set_errno(E_NO_SUCH_PROCESS);
        return -1;
    }

    if (pid == 1) {
        s_set_errno(E_TRIED_TO_KILL_INIT);
        return -1;
    }

    bool success = false;
    switch (signal) {
        case P_SIGINT:
            log_signaled(target->pid, target->priority, target->command);
            if (target->ignore_sigint) {
                return 0;
            }
            // fall through to P_SIGTERM
        case P_SIGTERM: { 
            // Terminate the process. Use a default status for now.
            // k_proc_exit handles moving to zombie queue and waking parent.
            int status = k_proc_exit(target, 1); // Using status 1 for killed by signal
            if (status != 0) {
                success = false;
                s_set_errno(status);
            }
            break;
        }

        case P_SIGTSTP:
            if (target->ignore_sigtstp) {
                return 0;
            }
            // fall through to P_SIGSTOP
        case P_SIGSTOP:
            // Stop the process
            if (target->pid != 1) {
                int status = k_stop_process(target);
                if (status != 0) {
                    success = false;
                    s_set_errno(status);
                }
            }
            break;

        case P_SIGCONT:
            // Continue a stopped process
            if (target->pid != 1) {
                int status = k_continue_process(target);
                if (status != 0) {
                    success = false;
                    s_set_errno(status);
                }
            }
            break;

        // Add cases for other signals as needed (SIGINT, SIGHUP, etc.)

        default:
            s_set_errno(E_INVALID_ARGUMENT);
            return -1;
    }
    if (success) {
        log_signaled(pid, target->priority, target->command ? target->command : "<?>");
    }
    return success ? 0 : -1;
}

/**
 * @brief Unconditionally exit the calling process with the given status.
 * This function does not return.
 * @param status The exit status code.
 */
void s_exit(int status) {
    pcb_t* current = k_get_current_process();
    if (current) {
        // Notify the kernel that this process is exiting
        int ret = k_proc_exit(current, status);
        if (ret != 0) {
            s_set_errno(ret);
        }
    } else {
        // Should not happen from a running process
        s_set_errno(E_INVALID_ARGUMENT);
    }
}

/**
 * @brief Set the priority of the specified process.
 *
 * @param pid Process ID of the target process.
 * @param priority The new priority value of the process (0, 1, or 2).
 * @return 0 on success, -1 on failure (e.g., process not found, invalid priority).
 */
int s_nice(pid_t pid, int priority) {
    pcb_t* target = k_get_process_by_pid(pid);
    if (!target) {
        s_set_errno(E_NO_SUCH_PROCESS);
        return -1;
    }

    // Validate priority (assuming enum values 0, 1, 2)
    if (priority < PRIORITY_HIGH || priority > PRIORITY_LOW) {
        s_set_errno(E_INVALID_ARGUMENT);
        return -1;
    }

    int status = k_set_priority(target, priority);
    if (status == 0) {
        log_nice(pid, target->priority, priority, target->command ? target->command : "<?>");
        return 0;
    } else {
        s_set_errno(status);
        return -1;
    }
}

/**
 * @brief Suspends execution of the calling process for a specified number of clock ticks.
 *
 * @param ticks Duration of the sleep in system clock ticks. Must be greater than 0.
 * @return 0 on success, -1 on error (e.g., invalid ticks, no current process).
 */
int s_sleep(unsigned int ticks) {
    if (ticks == 0) {
        // Sleeping for 0 ticks could be interpreted as a yield, but 
        // the description implies ticks > 0. We'll do nothing for 0.
        return 0; 
    }

    pcb_t* current = k_get_current_process();
    if (!current) {
        s_set_errno(E_NO_CURRENT_PROCESS);
        return -1; // Cannot sleep if not a process
    }

    // Call kernel sleep function
    int status = k_sleep(current, ticks);
    if (status == 0) {
        // If kernel successfully put process to sleep, yield the CPU
        spthread_suspend_self();
        // Execution resumes here after sleep duration (or signal)
    } else {
        s_set_errno(status);
        return -1;
    }

    // Call kernel sleep function
    // support re-entrancy
    while (k_resume_sleep(current)) {
        spthread_suspend_self();
    }
    return 0;
}

/**
 * @brief Get information about all processes. Implements `ps`.
 * 
 * This function retrieves and prints detailed information about all processes,
 * including their PID, PPID, priority, and state.
*/
void s_get_process_info() {
    k_get_all_process_info();
}

int s_tcsetpid(pid_t pid) {
    if (k_tcgetpid() == k_get_current_process()->pid) {
        k_tcsetpid(pid);
    } else {
        s_set_errno(E_TCSET_NO_TERMINAL_CONTROL);
        return -1;
    }
    return 0;
}

int s_ignore_sigint(bool ignore) {
    pcb_t* current = k_get_current_process();
    if (current == NULL) {
        s_set_errno(E_NO_CURRENT_PROCESS); // there should always at least be init as the current process
        return -1;
    }
    current->ignore_sigint = ignore;
    return 0;
}

int s_ignore_sigtstp(bool ignore) {
    pcb_t* current = k_get_current_process();
    if (current == NULL) {
        s_set_errno(E_NO_CURRENT_PROCESS); // there should always at least be init as the current process
        return -1;
    }
    current->ignore_sigtstp = ignore;
    return 0;
}

void s_logout() {
    k_logout();
}

// s_function to get the current process
pcb_t* s_get_current_process() {
    return k_get_current_process();
}

void s_set_errno(int errno) {
    pcb_t* current = k_get_current_process();
    current->errnumber = errno;
}

int s_get_errno() {
    pcb_t* current = k_get_current_process();
    return current->errnumber;
}

/**
 * @brief Initialize the scheduler.
 * Wrapper function.
 */
void s_init_scheduler() {
    init_scheduler();
}

/**
 * @brief Run the scheduler.
 * Wrapper function.
 */
void s_run_scheduler() {
    run_scheduler();
}
