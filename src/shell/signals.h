#ifndef SIGNALS_H_
#define SIGNALS_H_

#include <signal.h>
#include <sys/types.h>


// an exit status that indicates that the child was stopped (and not killed/interrupted/completed)
#define CHILD_STOPPED_EXIT_STATUS 99

// Global shell PID/PGID
extern pid_t shell_pgid;

/**
 * @brief Handler for SIGSTOP signals
 * When a SIGALRM is received and there's a child process,
 * sends SIGSTOP to that child process.
 */
void stop_handler(int sig);

/**
 * @brief Sets up the SIGALRM handler
 * @return 0 on success, -1 on error
 */
int setup_alarm_handler(void);

/**
 * @brief Handler for job control signals (SIGINT and SIGTSTP)
 * When a signal is received and there's a foreground job,
 * stops the job and adds it to the background jobs list
 */
void pennos_signal_handler(int sig);

/**
 * @brief Sets up handlers for SIGINT (Ctrl-C) and SIGTSTP (Ctrl-Z)
 * @return 0 on success, -1 on error
 */
int setup_job_control_handlers(void);

/**
 * @brief Ignores signals in the shell
 */
void ignore_signals(void);

/**
 * @brief Ignores SIGINT and SIGTSTP in the shell process
 */
void ignore_sigint(void);

#endif // SIGNALS_H_
