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
pid_t s_spawn(void* (*func)(void*), char *argv[], int fd0, int fd1) {
    // Get the parent process PCB using the kernel helper function.
    // This avoids direct access to scheduler_state from the system call layer.
    pcb_t* parent = k_get_current_process();

    // Directly call the kernel function to create the process.
    // k_proc_create now handles PCB setup, thread creation, and scheduling.
    pid_t new_pid = k_proc_create(parent, func, argv);

    // k_proc_create returns -1 on error, so we can return that directly.
    if (new_pid < 0) {
        // Optionally log the error at the syscall level if desired
        k_fprintf_short(STDERR_FILENO, "s_spawn: Failed to create process.\n");
        return new_pid;
    }
    
    if (parent != NULL) { // TODO: ideally we should handle this more gracefully so we don't have duplicate checks in k_proc_create and s_spawn
        if (fd0 < 0 || fd0 >= PROCESS_FD_TABLE_SIZE || fd1 < 0 || fd1 >= PROCESS_FD_TABLE_SIZE) {
            return S_SPAWN_INVALID_FD_ERROR;
        }
        process_fd_entry stdin_fd_entry = parent->process_fd_table[fd0];
        process_fd_entry stdout_fd_entry = parent->process_fd_table[fd1];
        if (stdin_fd_entry.in_use == false || stdout_fd_entry.in_use == false) {
            return S_SPAWN_INVALID_FD_ERROR;
        }

        // TODO: is this the right behavior? or is the intended behavior that the child process should inherit only the
        // *global* file descriptor that fd0 and fd1 (process level fds) point to?
        // The current implementation means that the child process will also inherit the mode and offset of the parent process
        k_get_process_by_pid(new_pid)->process_fd_table[STDIN_FD] = stdin_fd_entry;
        k_get_process_by_pid(new_pid)->process_fd_table[STDOUT_FD] = stdout_fd_entry; 
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
    return k_waitpid(pid, wstatus, nohang);
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
        return E_NO_SUCH_PROCESS;
    }

    if (pid == 1) {
        return E_TRIED_TO_KILL_INIT;
    }

    bool success = false;
    switch (signal) {
        case P_SIGTERM: 
            // Terminate the process. Use a default status for now.
            // k_proc_exit handles moving to zombie queue and waking parent.
            return k_proc_exit(target, 1); // Using status 1 for killed by signal
            break;

        case P_SIGSTOP:
            // Stop the process
            if (target->pid != 1) {
                success = k_stop_process(target);
            }
            break;

        case P_SIGCONT:
            // Continue a stopped process
            if (target->pid != 1) {
                success = k_continue_process(target);
            }
            break;

        // Add cases for other signals as needed (SIGINT, SIGHUP, etc.)

        default:
            return E_INVALID_ARGUMENT;
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
        k_proc_exit(current, status);
    } else {
        // Should not happen from a running process
        fprintf(stderr, "s_exit Error: Could not get current process!\n");
    }

    
    // Fallback infinite loop in case spthread_exit fails or isn't used.
    // A simple infinite loop is perhaps safest if spthread_exit isn't guaranteed.
    // while(1); 
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
        fprintf(stderr, "s_nice: Process PID %d not found.\n", pid);
        return -1;
    }

    // Validate priority (assuming enum values 0, 1, 2)
    if (priority < PRIORITY_HIGH || priority > PRIORITY_LOW) {
         fprintf(stderr, "s_nice: Invalid priority value %d for PID %d.\n", priority, pid);
         return -1;
    }

    if (k_set_priority(target, priority)) {
        log_nice(pid, target->priority, priority, target->command ? target->command : "<?>");
        return 0;
    } else {
        // k_set_priority might fail if internal state is inconsistent
        fprintf(stderr, "s_nice: Kernel failed to set priority for PID %d.\n", pid);
        return -1;
    }
}

/**
 * @brief Suspends execution of the calling process for a specified number of clock ticks.
 *
 * @param ticks Duration of the sleep in system clock ticks. Must be greater than 0.
 */
void s_sleep(unsigned int ticks) {
    if (ticks == 0) {
        // Sleeping for 0 ticks could be interpreted as a yield, but 
        // the description implies ticks > 0. We'll do nothing for 0.
        return; 
    }

    pcb_t* current = k_get_current_process();
    if (!current) {
        fprintf(stderr, "s_sleep Error: Could not get current process!\n");
        return; // Cannot sleep if not a process
    }

    // Call kernel sleep function
    if (k_sleep(current, ticks)) {
        // If kernel successfully put process to sleep, yield the CPU
        spthread_suspend_self();
        // Execution resumes here after sleep duration (or signal)
    } else {
         fprintf(stderr, "s_sleep Error: Kernel failed to put process PID %d to sleep.\n", current->pid);
         // Kernel function failed, maybe log error? Proceed without yielding.
    }
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

// s_function to get the current process
pcb_t* s_get_current_process() {
    return k_get_current_process();
}
