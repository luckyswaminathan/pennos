#include "scheduler.h"
#include "logger.h"
#include "../../lib/exiting_alloc.h"
#include "../../lib/linked_list.h"
#include "spthread.h"
#include "src/scheduler/fat_syscalls.h"

#define P_SIGTERM 1
#define P_SIGSTOP 2
#define P_SIGCONT 3

// This is a syscall level signal (it has no equivalent in the kernel)
#define P_SIGINT 4
#define P_SIGTSTP 5

#define S_SPAWN_INVALID_FD_ERROR -100

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
pid_t s_spawn(void* (*func)(void*), char *argv[], int fd0, int fd1, priority_t priority);

bool P_WIFEXITED(int wstatus);
bool P_WIFSTOPPED(int wstatus);
bool P_WIFSIGNALED(int wstatus);

#define W_EXITED 1 // 0b1
#define W_STOPPED 2 // 0b10

/**
 * @brief Wait on a child of the calling process, until it changes state.
 * If `nohang` is true, this will not block the calling process and return immediately.
 *
 * @param pid Process ID of the child to wait for.
 * @param wstatus Pointer to an integer variable where the status will be stored.
 * @param nohang If true, return immediately if no child has exited.
 * @return pid_t The process ID of the child which has changed state on success, -1 on error.
 */
pid_t s_waitpid(pid_t pid, int* wstatus, bool nohang);

/**
 * @brief Send a signal to a particular process.
 *
 * @param pid Process ID of the target proces.
 * @param signal Signal number to be sent.
 * @return 0 on success, -1 on error.
 */
int s_kill(pid_t pid, int signal);

/**
 * @brief Unconditionally exit the calling process with the given status.
 * This function does not return.
 * @param status The exit status code.
 */
void s_exit(int status);

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
int s_sleep(unsigned int ticks);

/**
 * @brief Get the process info of the current process and print it to stderr
 */
void s_get_process_info();

/**
 * @brief Set the terminal controlling process
 * @param pid process id of the new terminal controlling process
 * @return int 0 on success, or negative error code
 * @note If the current process is not the terminal controlling process, this will fail. Note also that the s_waitpid function
 * automatically passes terminal control to the child process when it is waited on with nohang set to false and pid set to the pid
 * of the child process. Thus it is expected that this function will rarely need to be called explicitly. Terminaly control is also
 * returned to the parent process on stop or termination.
 */
int s_tcsetpid(pid_t pid); 

/**
 * @brief Ignore or unignore SIGINT signals for the current process
 * @param ignore true to ignore, false to unignore
 * @return int 0 on success, or negative error code
 */
int s_ignore_sigint(bool ignore);

/**
 * @brief Ignore or unignore SIGTSTP signals for the current process
 * @param ignore true to ignore, false to unignore
 * @return int 0 on success, or negative error code
 */
int s_ignore_sigtstp(bool ignore);

/**
 * @brief Sets a flag to indicate that the logout command has been issued
 * and the scheduler should exit. Since this only signals the scheduler to exit,
 * it is not guaranteed to exit immediately.
 */
void s_logout();

/**
 * @brief Get the current process.
 * 
 * @return pcb_t* The current process.
 */
pcb_t* s_get_current_process();

/**
 * @brief Set the errno of the current process.
 * 
 * @param errno The errno to set.
 */
void s_set_errno(int errno);

/**
 * @brief Get the error number for the current process.
 * 
 * @return int The error number.
 */
int s_get_errno();
