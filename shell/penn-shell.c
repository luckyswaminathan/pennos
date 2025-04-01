#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h> 
#include "./parser.h"
#include "./command_execution.h"
#include "./shell_porcelain.h"
#include "./Job.h"
#include "./jobs.h"
#include "./exiting_alloc.h"
#include "./signals.h"
#include "../src/scheduler.h"

jid_t job_id = 0;


static void* shell_loop(void* arg) {
    while (true) {
        printf("Shell loop\n");

        display_prompt();  

        while (true) {
            pid_t dead_pid = waitpid(-1, NULL, WNOHANG | WUNTRACED);
            if (dead_pid == -1 && errno != ECHILD) {
                perror("Failed to wait for background jobs");
                exit(EXIT_FAILURE);
            }

            if (dead_pid == 0 || (dead_pid == -1 && errno == ECHILD)) {
                break;
            }

            remove_job_by_pid(dead_pid);
        }
     
        
        struct parsed_command *parsed_command = NULL;
        int ret = read_command(&parsed_command);

        if (ret == -1) {  
            exit(0);
        } else if (ret == -2) {  
            if (errno == EINTR) {
    
                continue;
            }
            perror("Error reading command");
            exit(1);
        }

        if (parsed_command == NULL) {
            continue;  // Empty command, try again
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
        fprintf(stderr, "job_id: %lu\n", job_ptr->id);
        job_ptr->pids = NULL;
        job_ptr->status = J_RUNNING_FG;
        job_ptr->cmd = parsed_command;

        if (parsed_command->is_background) {
            job_ptr->status = J_RUNNING_BG;
            execute_job(job_ptr);
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

// Thread function to run the scheduler
void* scheduler_thread_fn(void* arg) {
    run_scheduler();
    return NULL;
}

int main(int argc, char **argv) {
    // First ignore signals
    ignore_signals();
    
    // Initialize shell's process group
    init_shell_pgid();

    // Initialize scheduler
    init_scheduler();
    
    // Finally set up the job control handlers
    setup_job_control_handlers();

    printf("Shell PID/PGID: %d; getpid(): %d\n", shell_pgid, getpid());

    // Create scheduler thread
    spthread_t scheduler_thread;
    if (spthread_create(&scheduler_thread, NULL, scheduler_thread_fn, NULL) != 0) {
        perror("Failed to create scheduler thread");
        exit(1);
    }

    // Create shell thread
    spthread_t shell_thread;
    if (spthread_create(&shell_thread, NULL, shell_loop, NULL) != 0) {
        perror("Failed to create shell thread");
        exit(1);
    }

    // Create shell PCB and make it ready
    pcb_t* shell_pcb = create_process(shell_thread, 0, false);
    shell_pcb->non_preemptible = true;  // Don't preempt the shell
    make_process_ready(shell_pcb);
    
    // Start the shell thread
    spthread_continue(shell_thread);

    // Wait for shell thread to finish
    spthread_join(shell_thread, NULL);

    return EXIT_SUCCESS;
}
