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
#include "../../lib/exiting_alloc.h"
#include "./signals.h"
#include "../scheduler/scheduler.h"
#include "../scheduler/sys.h"
#include "commands.h"
#include "src/pennfat/fat.h"

jid_t job_id = 0;

static void* shell_loop(void* arg) {
    while (true) {
        display_prompt();
     
        struct parsed_command *parsed_command = NULL;
        int ret = read_command(&parsed_command);

        // Error handling
        if (ret == -1) {
            exit(0);
        } else if (ret == -2) {
            if (errno == EINTR) {
                continue;
            }
            perror("Error reading command");
            exit(1);
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

        job* job_ptr = (job*) exiting_malloc(sizeof(job));
        job_ptr->id = ++job_id;
        job_ptr->pids = NULL;
        job_ptr->status = J_RUNNING_FG;
        job_ptr->cmd = parsed_command;

        if (parsed_command->is_background) {
            job_ptr->status = J_RUNNING_BG;
            execute_job(job_ptr);
            //s_waitpid(-1, NULL, true);
            enqueue_job(job_ptr);
        } else {
            // status is already J_RUNNING_FG
            add_foreground_job(job_ptr);

            // TODO: remove
            handle_jobs();

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
    s_spawn(shell_loop, (char*[]){"shell", NULL}, STDIN_FILENO, STDOUT_FILENO, PRIORITY_HIGH);

    // Consume any zombies
    int wstatus;
    while (s_waitpid(-1, &wstatus, true) > 0) {}

    return NULL;
}

int main(int argc, char **argv) {
    int mount_status = mount("pennosfat");
    if (mount_status != 0) {
        exit(mount_status);
    }

    // First ignore signals
    ignore_signals();
    
    // Initialize shell's process group
    init_shell_pgid();

    // Initialize logger and scheduler
    init_logger("scheduler.log");
    init_scheduler();

    // Spawn init process
    s_spawn(init_process, (char*[]){"init", NULL}, STDIN_FILENO, STDOUT_FILENO, PRIORITY_HIGH);
    printf("Scheduler initialized\n");
    
    // Finally set up the job control handlers
    setup_job_control_handlers();

    run_scheduler();

    return EXIT_SUCCESS;
}
