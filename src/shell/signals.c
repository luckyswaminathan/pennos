#include "./signals.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <stdbool.h>
#include <signal.h>
#include "./Job.h"
#include "./jobs.h"
#include "../../lib/linked_list.h"
#include "./command_execution.h"
#include "../scheduler/sys.h"

// Define the global variable here
pid_t shell_pgid;

// Add initialization function
void init_shell_pgid(void) {
    shell_pgid = getpid();
    setpgid(shell_pgid, shell_pgid);
    tcsetpgrp(STDIN_FILENO, shell_pgid);
}

void job_control_handler(int sig) {
    // Use shell_pgid directly instead of getpid()
    
    // Get the actual foreground job's PGID
    job* job = get_jobs_head();
    if (job && job->pids && job->status == J_RUNNING_FG) {
        if (sig == SIGTSTP) {
            // Use PennOS s_stop instead of real kill system call
            s_kill(scheduler_state->current_process->pid, P_SIGSTOP);
            
            // Don't wait here - let the parent's waitpid handle it
            job->status = J_STOPPED;
            
            // Remove from foreground and add to background jobs list
            remove_foreground_job(job);
            enqueue_job(job);
            
            // TODO: remove
            handle_jobs();
            
            // Make sure terminal is in a good state
            tcsetpgrp(STDIN_FILENO, shell_pgid);
            
            exit(CHILD_STOPPED_EXIT_STATUS); // TODO: this is just a POC. Not sure if there's a smarter way of doing this. Maybe we don't need a signal handler at all and can just catch WIFSTOPPED
        } else if (sig == SIGINT) {
            // Use only PennOS s_kill to properly terminate PennOS processes

            s_kill(scheduler_state->current_process->pid, P_SIGTERM);
            
            // Make sure the terminal control is returned to the shell
            tcsetpgrp(STDIN_FILENO, shell_pgid);
        }
    }
}

int setup_job_control_handlers(void) {
    struct sigaction act = {
        .sa_flags = SA_RESTART,
        .sa_handler = job_control_handler
    };

    if (sigaction(SIGINT, &act, NULL) < 0) {
        perror("sigaction for SIGINT failed");
        return -1;
    }

    if (sigaction(SIGTSTP, &act, NULL) < 0) {
        perror("sigaction for SIGTSTP failed");
        return -1;
    }

    return 0;
}

void ignore_signals(void) {
    struct sigaction sigac;
    sigac.sa_handler = SIG_IGN;
    sigemptyset(&sigac.sa_mask);
    sigac.sa_flags = 0;

    struct sigaction sa_continue = {
        .sa_handler = SIG_DFL,
        .sa_flags = 0
    };
    
    // Ignore these signals in the shell
    sigaction(SIGINT, &sigac, NULL);
    sigaction(SIGTSTP, &sigac, NULL);
    sigaction(SIGTTOU, &sigac, NULL);
    sigaction(SIGTTIN, &sigac, NULL);
    sigaction(SIGCHLD, &sa_continue, NULL);
    
    // Remove process group setup from here since it's now in main
}

void ignore_sigint(void) {
    struct sigaction sigac;
    sigac.sa_handler = SIG_IGN;
    sigemptyset(&sigac.sa_mask);
    sigac.sa_flags = 0;
    
    // Ignore SIGINT and SIGTSTP (not SIGSTOP) in the shell process
    sigaction(SIGINT, &sigac, NULL);
    sigaction(SIGTSTP, &sigac, NULL);  // Changed from SIGSTOP to SIGTSTP
    
    sigaction(SIGTTOU, &sigac, NULL);
}
