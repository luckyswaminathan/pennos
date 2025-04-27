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

/**
 * Signal handler for PennOS to accept host OS signals and 
 * forward them appropriately.
 */
void pennos_signal_handler(int sig) {
    // Use shell_pgid directly instead of getpid()
    
    // Get the actual foreground job's PGID
    if (sig == SIGTSTP) {
        // forward it to the foreground process
        s_kill(scheduler_state->terminal_controlling_pid, P_SIGTSTP);
        return;
    } else if (sig == SIGINT) {
        // forward it to the foreground process
        s_kill(scheduler_state->terminal_controlling_pid, P_SIGINT);
        return;
    }
}

int setup_job_control_handlers(void) {
    struct sigaction act = {
        .sa_flags = SA_RESTART,
        .sa_handler = pennos_signal_handler
    };

    if (sigaction(SIGINT, &act, NULL) < 0) {
        k_log("sigaction for SIGINT failed");
        return -1;
    }

    if (sigaction(SIGTSTP, &act, NULL) < 0) {
        k_log("sigaction for SIGTSTP failed");
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
