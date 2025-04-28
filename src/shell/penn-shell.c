#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h> 
#include "./parser.h"
#include "../scheduler/logger.h"
#include "./command_execution.h"
#include "./shell_porcelain.h"
#include "./Job.h"
#include "./jobs.h"
#include "./signals.h"
#include "../scheduler/sys.h"
#include "commands.h"
#include "src/pennfat/fat.h"
#include <string.h>
#include "src/scheduler/fat_syscalls.h"
#include "./jobs.h"

jid_t job_id = 0;

static void* shell_loop(void* arg) {
    s_ignore_sigint(true);
    s_ignore_sigtstp(true);
    while (true) {
        pid_t pid;
        int status;
        while ((pid = s_waitpid(-1, &status, true)) > 0) {
            if (P_WIFSTOPPED(status)) {
                job* job = find_job_by_pid(pid);
                if (job != NULL) {
                    job->status = J_STOPPED;
                    remove_job_by_pid(pid);
                    enqueue_job(job);
                }
            } else {
                job* job = find_job_by_pid(pid);
                if (job != NULL) {
                    // Print completion message before removing/destroying
                    s_fprintf_short(STDERR_FILENO, "[%lu] Done ", job->id);
                    print_job_command(job); // Uses fprintf, prints command without newline
                    s_fprintf_short(STDERR_FILENO, "\n"); // Add newline
                    remove_job_by_pid(pid); // Removes job from list and destroys it
                }
            }
        }

        display_prompt();
     
        struct parsed_command *parsed_command = NULL;
        int ret = read_command(&parsed_command);


        if (ret < 0) {
            s_logout();
            continue;
        } else if (ret == -2) {  
            if (errno == EINTR) {
                continue;
            }
            char* error_message = "Error reading command\n";
            s_write(STDERR_FILENO, error_message, strlen(error_message));
            continue;
        }

        // Empty command, try again
        if (parsed_command == NULL) {
            continue;
        }

        if (parsed_command->num_commands <= 0 || parsed_command->commands[0][0] == NULL) {
            free(parsed_command);
            continue; // do nothing and try reading again
        }
        bool is_jobs_command = handle_jobs_commands(parsed_command);

        if (is_jobs_command) {
            // jobs commands are handled by the shell, and not execve'd
            free(parsed_command);
            continue;
        }

        job* job_ptr = (job*) malloc(sizeof(job));
        if (!job_ptr) {
            s_fprintf_short(STDERR_FILENO, "Failed to allocate job\n");
            free(parsed_command);
            continue;
        }
        job_ptr->id = ++job_id;
        job_ptr->pid = -1;
        job_ptr->status = J_RUNNING_FG;
        job_ptr->cmd = parsed_command;

        if (parsed_command->is_background) {
            job_ptr->status = J_RUNNING_BG;
            execute_job(job_ptr);
            // Add logging for background job start
            if (job_ptr->pid > 0) { // Ensure PID is valid before printing
                s_fprintf_short(STDERR_FILENO, "[%lu] %d\n", job_ptr->id, job_ptr->pid);
            }
            //s_waitpid(-1, NULL, true);
            enqueue_job(job_ptr);
        } else {
            // status is already J_RUNNING_FG
            add_foreground_job(job_ptr);

            execute_job(job_ptr); 
            // Only destroy the job if it wasn't stopped
            if (job_ptr->status != J_STOPPED) {
                remove_foreground_job(job_ptr);
                destroy_job(job_ptr);
            }
            // Don't display prompt here - it will be displayed at the start of the next loop
        }
    }
    return NULL;
}

/**
 * @brief Spawns the shell process
 * 
 * This process is responsible for spawning the shell process
 * and waiting for it to exit.
 * 
 * @param arg 
 * @return void* 
 */
static void* init_process(void* arg) {
    // Spawn shell process
    pid_t pid = s_spawn(shell_loop, (char*[]){"shell", NULL}, STDIN_FILENO, STDOUT_FILENO, PRIORITY_HIGH);
    s_tcsetpid(pid);

    // Consume any zombies
    int wstatus;
    pid_t result;
    while (true) {
        while ((result = s_waitpid(-1, &wstatus, true)) > 0) {
        }
    }

    return NULL;
}

int main(int argc, char **argv) {
    if (argc < 2 || argc > 3) {
        perror("Usage: pennos fatfs [log_fname]"); // NOTE: this usage is OK because we haven't started PennOS yet
        exit(EXIT_FAILURE);
    }

    // Initialize fat filesystem
    int mount_status = mount(argv[1]);
    if (mount_status != 0) {
        exit(mount_status);
    }

    // First ignore signals
    ignore_signals();

    // Initialize logger and scheduler
    if (argc == 3) {
        init_logger(argv[2]);
    } else {
        init_logger("log");
    }
    if (init_scheduler() == -1) {
        exit(EXIT_FAILURE);
    }

    // Spawn init process
    pid_t pid = s_spawn(init_process, (char*[]){"init", NULL}, STDIN_FILENO, STDOUT_FILENO, PRIORITY_HIGH);
    k_tcsetpid(pid);
    
    // Finally set up the job control handlers
    setup_job_control_handlers();

    s_run_scheduler();

    return EXIT_SUCCESS;
}
