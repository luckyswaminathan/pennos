#include "./command_execution.h"
#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <wait.h>
#include "./exiting_signal.h"
#include "./print.h"
#include <sys/stat.h>
#include <fcntl.h>
#include "./Job.h"
#include <assert.h> // TODO: remove before submission
#include "./valid_input.h"
#include "./signals.h"  // Add this to access shell_pgid
#include "./jobs.h"
#include "commands.h"
#include "../scheduler/sys.h"
#include "../../lib/exiting_alloc.h"

#define DEFAULT_FILE_PERMISSIONS 0644  // User: read/write, Group: read, Others: read
#define FORK_SETUP_DELAY_USEC 500     // Microseconds to wait for child process setup

/**
 * Execute the "lead" child, which executes all the other commands.
 */

pid_t current_pid;

void execute_job(job* job)
{
    struct parsed_command* parsed_command = job->cmd;
    validate_command(parsed_command);
    
    // No need to store shell's PGID locally anymore
    
    // Allocate space for PIDs
    job->pids = malloc(sizeof(pid_t) * parsed_command->num_commands);
    if (!job->pids) {
        perror("Failed to allocate PIDs array");
        return;
    }

    struct command_context* context = exiting_malloc(sizeof(struct command_context));
    printf("Executing command: %s\n", parsed_command->commands[0][0]);
    context->command = parsed_command->commands[0];
    context->stdin_fd = STDIN_FILENO;
    context->stdout_fd = STDOUT_FILENO;

    pid_t pid = s_spawn((void* (*)(void*))execute_command,
                          context->command,  // argv
                          context->stdin_fd, // fd0
                          context->stdout_fd // fd1
                         );
    if (pid == -1)
    {
        perror("Failed to spawn command");
        exit(EXIT_FAILURE);
    }
    
    // Store the lead process ID
    // job->pids[0] = pid;
    // job->num_processes = parsed_command->num_commands;



    if (job->status == J_RUNNING_FG) {   
        int status;
        s_waitpid(pid, &status, false);
        
        // TODO: don't love putting this logic here
        // Since we handle the signals in the child, we can't directly check for WIFSTOPPED. Instead, we exit with a sentinel
        // status code from the child.
        if (WIFEXITED(status) && WEXITSTATUS(status) == CHILD_STOPPED_EXIT_STATUS) {
            job->status = J_STOPPED;
            remove_foreground_job(job);
            enqueue_job(job);
        }

        // Use global shell_pgid
        // TODO: cannot use tcsetpgrp, should instead track pid that can use stdin
    } else if (job->status == J_RUNNING_BG) {
        printf("job %lu is running in the background\n", job->id);
        return;
    }
}
