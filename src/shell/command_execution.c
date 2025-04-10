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

#define DEFAULT_FILE_PERMISSIONS 0644  // User: read/write, Group: read, Others: read
#define FORK_SETUP_DELAY_USEC 500     // Microseconds to wait for child process setup

/**
 * Execute the "lead" child, which executes all the other commands.
 */

pid_t current_pid;




void execute_job_lead_child(job* job, struct parsed_command* parsed_command) {

    // this is the child process that is the leader of the job's process group
    {
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

        pid_t pid = fork();
        if (pid == 0)
        {

            // NOTE: if the file descriptor is already correct, than we don't call dup2
            if (command_input_fd != STDIN_FILENO && dup2(command_input_fd, STDIN_FILENO) == -1)
            {
                perror("Failed to redirect stdin for command in job");
                exit(EXIT_FAILURE); // Exit the child process
            }
            if (command_output_fd != STDOUT_FILENO && dup2(command_output_fd, STDOUT_FILENO) == -1)
            {
                perror("Failed to redirect stdout for command in job");
                exit(EXIT_FAILURE);
            }

            // close the next_command_input_fd since it's not relevant to this command
            if (next_command_input_fd != -1)
            {
                close(next_command_input_fd);
            }

            // NOTE: it's safe to fork in this loop because we always call execvp or exit in the child
            execvp(parsed_command->commands[i][0], parsed_command->commands[i]);
            // if we've reached this point, we have an error
            perror("Failed to execve command.");
            // TODO: need to close fds here?
            exit(EXIT_FAILURE); // exits out the child for the command (doesn't exit out of the job) // TODO: is this enough? presumably if one part of the pipeline fails, the rest should fail, right? check this
        }

        if (pid == -1)
        {
            perror("Failed to fork off a command while executing a job");
            exit(EXIT_FAILURE); // exit out of the job process // TODO: should we exit here?
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
        pid_t dead_pid = waitpid(-1, &status, WUNTRACED);

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
    
    pid_t pid = fork();
    if (pid == 0) {
        // Child process
        setpgid(0, 0);  // Create new process group in child
        execute_job_lead_child(job, parsed_command);
    }

    if (pid == -1) {
        perror("Failed to fork while executing a job");
        free(job->pids);
        job->pids = NULL;
        return;
    }

    // Store the lead process ID
    job->pids[0] = pid;
    job->num_processes = parsed_command->num_commands;



    if (job->status == J_RUNNING_FG) {   
        tcsetpgrp(STDIN_FILENO, pid);
        int status;
        waitpid(pid, &status, WUNTRACED);
        
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
