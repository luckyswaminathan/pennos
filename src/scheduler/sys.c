#include "scheduler.h"
#include "kernel.h"
#include "logger.h"
#include "../../lib/exiting_alloc.h"
#include "../../lib/linked_list.h"
#include "spthread.h"
#include <stdlib.h> // For NULL
#include <string.h> // For strcmp etc. if needed
#include <signal.h> // For signal definitions (SIGTERM, SIGSTOP, SIGCONT)
#include <stdio.h>  // For fprintf
#include <unistd.h> // For _exit? No, use spthread_exit or infinite loop.

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

    // 2. Call the kernel implementation in a loop to handle blocking
    pid_t result = -1; 
    while (1) {
        result = k_waitpid(parent, pid, wstatus, nohang);

        if (result == -2) { // Special code for blocking
            // Kernel indicated blocking is required. Yield and retry.
            if (nohang) { 
                // Should not happen if nohang is true, k_waitpid should return 0 instead.
                // Treat as error or unexpected state.
                fprintf(stderr, "s_waitpid Warning: k_waitpid requested blocking when nohang was set.\n");
                return -1; 
            }
            k_yield(); // Give up CPU, will resume here later
            // Loop back to call k_waitpid again
        } else {
            // Got a definitive result: PID, 0 (nohang), or -1 (error)
            break; // Exit the loop
        }
    }
    
    // Return the final result from k_waitpid
    return result; 
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
        fprintf(stderr, "s_kill: Process PID %d not found.\n", pid);
        return -1; // ESRCH (No such process)
    }

    bool success = false;
    switch (signal) {
        case SIGTERM: 
            // Terminate the process. Use a default status for now.
            // k_proc_exit handles moving to zombie queue and waking parent.
            fprintf(stdout, "s_kill: Sending SIGTERM to PID %d\n", pid);
            k_proc_exit(target, 1); // Using status 1 for killed by signal
            success = true; // k_proc_exit doesn't return status, assume success if target found
            break;

        case SIGSTOP:
            // Stop the process
            fprintf(stdout, "s_kill: Sending SIGSTOP to PID %d\n", pid);
            success = k_stop_process(target);
            break;

        case SIGCONT:
            // Continue a stopped process
             fprintf(stdout, "s_kill: Sending SIGCONT to PID %d\n", pid);
            success = k_continue_process(target);
            break;

        // Add cases for other signals as needed (SIGINT, SIGHUP, etc.)

        default:
            fprintf(stderr, "s_kill: Signal %d not supported.\n", signal);
            return -1; // EINVAL (Invalid argument)
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

    // Ensure the thread terminates and doesn't return from s_exit.
    // Using spthread_exit is appropriate here if available and intended.
    // Alternatively, an infinite loop prevents return, relying on the scheduler 
    // to never schedule this zombie process again.
    spthread_exit(NULL); // Use spthread library's exit mechanism
    
    // Fallback infinite loop in case spthread_exit fails or isn't used.
    // while(1) { sleep(1000); } // sleep() is forbidden, use busy wait or yield
    // while(1) { k_yield(); } // Yielding might still allow cleanup issues
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
        k_yield(); 
        // Execution resumes here after sleep duration (or signal)
    } else {
         fprintf(stderr, "s_sleep Error: Kernel failed to put process PID %d to sleep.\n", current->pid);
         // Kernel function failed, maybe log error? Proceed without yielding.
    }
}

void s_get_process_info() {
    k_get_all_process_info();
}