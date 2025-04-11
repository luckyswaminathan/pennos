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
#include "../scheduler/scheduler.h"
#include "context.h"
#include "../scheduler/sys.h"
#include "../../lib/exiting_alloc.h"

#define DEFAULT_FILE_PERMISSIONS 0644  // User: read/write, Group: read, Others: read
#define FORK_SETUP_DELAY_USEC 500     // Microseconds to wait for child process setup

/**
 * Execute the "lead" child, which executes all the other commands.
 */

pid_t current_pid;




void execute_job_lead_child(job* job, struct parsed_command* parsed_command) {

    // this is the child process that is the leader of the job's process group
    {
        // TODO: change this to do smth with the scheduler
        int ret = setpgid(0, 0);
        if (ret == -1)
        {
            perror("Failed to create a process group to execute the job");
            exit(EXIT_FAILURE); // this exits out of the child process, but not the parent
        }
    }

    int job_input_fd = STDIN_FILENO;
    if (parsed_command->stdin_file != NULL)
    {
        int file_descriptor = open(parsed_command->stdin_file, O_RDONLY, DEFAULT_FILE_PERMISSIONS);
        if (file_descriptor == -1)
        {
            perror("Failed to open input file");
            exit(EXIT_FAILURE); // Exit the job process
        }
        job_input_fd = file_descriptor;
    }

    int job_output_fd = STDOUT_FILENO;
    if (parsed_command->stdout_file != NULL)
    {
        int file_descriptor = open(parsed_command->stdout_file, O_WRONLY | O_CREAT | O_TRUNC, DEFAULT_FILE_PERMISSIONS);
        if (file_descriptor == -1)
        {
            perror("Failed to open output file");
            exit(EXIT_FAILURE); // Exit the job process
        }
        job_output_fd = file_descriptor;
    }

    int command_input_fd = job_input_fd; // this is always start for each iteration
    int command_output_fd;               // this is never set before each iteration; we set it in the loop
    for (size_t i = 0; i < parsed_command->num_commands; i++)
    {
        int next_command_input_fd;
        if (i == parsed_command->num_commands - 1)
        {
            command_output_fd = job_output_fd;
            next_command_input_fd = -1;
        }
        else
        {
            int pipe_fds[2];
            pipe(pipe_fds);
            command_output_fd = pipe_fds[1];     // write end
            next_command_input_fd = pipe_fds[0]; // read end
        }

        struct command_context* context = exiting_malloc(sizeof(struct command_context));
        context->command = parsed_command->commands[i];
        context->stdin_fd = command_input_fd;
        context->stdout_fd = command_output_fd;
        context->next_input_fd = next_command_input_fd;

        pid_t pid = s_spawn((void* (*)(void*))execute_command, context->command, context->stdin_fd, context->stdout_fd);
        if (pid == -1)
        {
            perror("Failed to spawn command");
            exit(EXIT_FAILURE);
        }

        

        // TODO: add the pid to the job struct
        current_pid = pid;
        job->pids[i] = pid;
        // close them on the parent process
        // NOTE: we don't close stdin or stdout
        if (command_input_fd != STDIN_FILENO)
        {
            if (close(command_input_fd) < 0)
            {
                perror("Failed to close stdin file descriptor");
                // TODO: exit here?
            }
        }
        if (command_output_fd != STDOUT_FILENO)
        {
            if (close(command_output_fd) < 0)
            {
                perror("Failed to close stdout file descriptor");
                // TODO: exit here?
            }
        }

        command_input_fd = next_command_input_fd;
    }

    // poll for all children to finish or stop
    while (true) {
        int status;
        pid_t dead_pid = s_waitpid(-1, &status, true);

        if (dead_pid == -1) {
            if (errno == ECHILD) {
                break;  // No more children
            }
            perror("waitpid failed");
            exit(1);
        }
        
        // TODO: when does this happen
        // If process stopped, we can exit
        if (WIFSTOPPED(status)) {
            exit(0);
        }
    }

    exit(0);
}

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
    context->command = parsed_command->commands[0];
    context->stdin_fd = STDIN_FILENO;
    context->stdout_fd = STDOUT_FILENO;
    context->next_input_fd = -1;
    pid_t pid = s_spawn((void* (*)(void*))execute_command, context->command, context->stdin_fd, context->stdout_fd);
    if (pid == -1)
    {
        perror("Failed to spawn command");
        exit(EXIT_FAILURE);
    }
    // Store the lead process ID
    job->pids[0] = pid;
    job->num_processes = parsed_command->num_commands;



    if (job->status == J_RUNNING_FG) {   
        tcsetpgrp(STDIN_FILENO, pid);
        int status;
        s_waitpid(pid, &status, true);
        
        // TODO: don't love putting this logic here
        // Since we handle the signals in the child, we can't directly check for WIFSTOPPED. Instead, we exit with a sentinel
        // status code from the child.
        if (WIFEXITED(status) && WEXITSTATUS(status) == CHILD_STOPPED_EXIT_STATUS) {
            printf("Child was stopped in execute_command");
            job->status = J_STOPPED;
            remove_foreground_job(job);
            enqueue_job(job);
        }

        // Use global shell_pgid
        tcsetpgrp(STDIN_FILENO, shell_pgid);
    } else if (job->status == J_RUNNING_BG) {
        printf("job %lu is running in the background\n", job->id);
        return;
    }
}
