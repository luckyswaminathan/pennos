#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include "commands.h"
#include "../scheduler/scheduler.h"
#include "../scheduler/logger.h"
#include "../scheduler/sys.h"
#include "../../lib/exiting_alloc.h"
#include "../scheduler/spthread.h"
#include "src/pennfat/fat_constants.h"
#include "jobs.h"
#include "stress.h"
#define BUFFER_SIZE 256

// Helper function specific to ps command within this file
static void print_process_line(pcb_t* proc, char state_char) {
     if (!proc) return;
     // Print process info directly using printf
     printf("%3d %4d %3d %c    %s\n", 
            proc->pid, 
            proc->ppid, 
            proc->priority, 
            state_char, 
            proc->command ? proc->command : "<?>" // Use command name
     );
}

// static void print_header(int output_fd) {
//     const char* header = "PID PPID PRI STAT CMD\n";
//     dprintf(output_fd, "%s", header);
//     LOG_INFO("%s", header);
// }
// static void print_process(pcb_t* proc, int output_fd) {
//     char state_char = 'R';
//     if (proc->state == PROCESS_BLOCKED){ state_char = 'B';
//     } else if (proc->state == PROCESS_ZOMBIED) {
//         state_char = 'Z';
//     }
//     dprintf(output_fd, "%3d %4d %3d %c   %s\n", proc->pid, proc->ppid, proc->priority, state_char, proc->command);
//     LOG_INFO("Process info - PID: %d, PPID: %d, STATE: %d, COMMAND: %s", proc->pid, proc->ppid, proc->state, proc->command);
// }



// Implementation of ps command
void* ps(void* arg) {
    // Directly access kernel state (assuming this is allowed by the project structure)
    if (!scheduler_state) {
        printf("Scheduler not initialized.\n");
        s_exit(1); // Exit with an error code
        return NULL; 
    }

    // Print header
    printf("PID PPID PRI STAT CMD\n");

    // Get current process - careful, this is the 'ps' process itself
    // pcb_t *ps_process = scheduler_state->current_process; 
    // pid_t ps_pid = (ps_process) ? ps_process->pid : -1;


    // --- Iterate through all queues ---

    // Ready Queues (excluding the ps process itself)
    // TODO: Maintain abstraction here
    for (int i = 0; i < 3; i++) {
        pcb_t* current = scheduler_state->ready_queues[i].head;
        while (current != NULL) {
            print_process_line(current, 'R');
            current = current->next;
        }
    }

    // Blocked Queue
    pcb_t* current = scheduler_state->blocked_queue.head;
    while (current != NULL) {
        print_process_line(current, 'B');
        current = current->next;
    }

    // Stopped Queue
    current = scheduler_state->stopped_queue.head;
    while (current != NULL) {
        print_process_line(current, 'S');
        current = current->next;
    }

    // Zombie Queue
    current = scheduler_state->zombie_queue.head;
    while (current != NULL) {
        print_process_line(current, 'Z');
        current = current->next;
    }

    s_exit(0); // Exit the ps process itself cleanly
    return NULL;
}

void* zombie_child(void* arg) {
    // Child process exits normally
    // LOG_INFO("Child process running, will exit soon");
    s_exit(0);
    return NULL;
}


void* zombify(void* arg) {

    // Spawn the child process
    s_spawn(zombie_child, (char*[]){"zombie_child", NULL}, STDIN_FILENO, STDOUT_FILENO, PRIORITY_MEDIUM);
    s_get_process_info();
    while(1) {  
    };
    s_exit(0);
    return NULL;
}

void* orphan_child(void* arg) {
    
    while(1) {
    };
    s_exit(0);
    return NULL;
}

void* orphanify(void* arg) {
    s_spawn(orphan_child, (char*[]){"orphan_child", NULL}, STDIN_FILENO, STDOUT_FILENO, PRIORITY_MEDIUM);
    s_exit(0);
    return NULL;
}

/**
 * @brief This function is used to busy wait a process.
 * 
 * It removes the current process from the ready queue and adds it to the ready queue with the new priority.
 * It then busy waits.
 * 
 * @param arg The command context.
 * @param priority The priority to set the process to.
 * @return NULL.
 */
void* busy(void* arg, char* priority) {
    // Get the command context
    //char** command = (char**)arg;

    printf("priority: %s\n", priority);

    int priority_level = atoi(priority);

    // Validate the priority level
    if (priority_level < 0 || priority_level > 2) {
        s_write(STDERR_FILENO, "Invalid priority level. Use 0 (high), 1 (medium), or 2 (low)\n", strlen("Invalid priority level. Use 0 (high), 1 (medium), or 2 (low)\n"));
        s_exit(1);
        return NULL;
    }

    // Remove the current process from the ready queue and add it to the ready queue with the new priority
    pcb_t* proc = scheduler_state->current_process;
    linked_list_remove(&scheduler_state->ready_queues[proc->priority], proc);
    proc->priority = priority_level;
    linked_list_push_tail(&scheduler_state->ready_queues[priority_level], proc);

    // Busy wait
    while (1) {}

    s_exit(0);
    return NULL;
}

// Implementation of nice command to use the system s_nice function
void* nice_pid_command(void* arg, char* pid, char* priority) {
    //char** command = (char**)arg;
    //int stdout_fd = command[1];
    
    if (pid == NULL || priority == NULL) {
        s_write(STDERR_FILENO, "Usage: nice <pid> <priority_level>\n", strlen("Usage: nice <pid> <priority_level>\n"));
        return NULL;
    }
    
    // Parse PID and priority level
    pid_t target_pid = atoi(pid);
    int priority_level = atoi(priority);
    
    // Validate priority level
    if (priority_level < 0 || priority_level > 2) {
        s_write(STDERR_FILENO, "Invalid priority level. Use 0 (high), 1 (medium), or 2 (low)\n", strlen("Invalid priority level. Use 0 (high), 1 (medium), or 2 (low)\n"));
        return NULL;
    }
    
    // Call the system nice function
    int result = s_nice(target_pid, priority_level);
    
    if (result == 0) {
        char output_string[100];
        sprintf(output_string, "Changed priority of process %d to %d\n", target_pid, priority_level);
        s_write(STDERR_FILENO, output_string, strlen(output_string));
    } else {
        char output_string[100];
        sprintf(output_string, "Failed to change priority of process %d\n", target_pid);
        s_write(STDERR_FILENO, output_string, strlen(output_string));
    }
    s_exit(0);
    return NULL;
}

void* nice_command(void* arg, char* priority) {
    char** command = (char**)arg;
    //int stdout_fd = command[1];
    
    if (priority == NULL) {
        s_write(STDERR_FILENO, "Usage: nice <priority_level>\n", strlen("Usage: nice <priority_level>\n"));
        return NULL;
    }
    
    // Parse priority level
    int priority_level = atoi(priority);
    
    // Validate priority level
    if (priority_level < 0 || priority_level > 2) {
        s_write(STDERR_FILENO, "Invalid priority level. Use 0 (high), 1 (medium), or 2 (low)\n", strlen("Invalid priority level. Use 0 (high), 1 (medium), or 2 (low)\n"));
        return NULL;
    }
    
    char* call = command[2];
    char output_string[100];
    sprintf(output_string, "call value: %s\n", call);
    s_write(STDERR_FILENO, output_string, strlen(output_string));
    s_exit(0);
    return NULL;
}

void* sleep_command(void* arg, char* time) {
    if (time == NULL) {
        s_write(STDERR_FILENO, "Error: sleep command requires a number of ticks\n", strlen("Error: sleep command requires a number of ticks\n"));
        s_exit(1);
        return NULL;
    }
    int ticks = atoi(time);
    s_sleep(ticks);
    s_exit(0);
    return NULL;
}

void* kill_process_shell(void* arg, char* first_term) {
    char** args = (char**)arg;
    if (args == NULL || first_term == NULL) {
        s_write(STDERR_FILENO, "Error: kill command requires a process ID\n", strlen("Error: kill command requires a process ID\n"));
        s_exit(1);
        return NULL;
    }
    int signal = P_SIGTERM; // Default signal
    int start_idx = 1; // Default to first argument as PID

    // Check if we have a signal flag
    if (first_term != NULL && first_term[0] == '-') {
        if (strcmp(first_term, "-term") == 0) {
            signal = P_SIGTERM;
        } else if (strcmp(first_term, "-stop") == 0) {
            signal = P_SIGSTOP;
        } else if (strcmp(first_term, "-cont") == 0) {
            signal = P_SIGCONT;
        } else {
            s_write(STDERR_FILENO, "Unknown signal flag\n", strlen("Unknown signal flag\n"));
            s_write(STDERR_FILENO, "Usage: kill [-term|-stop|-cont] <pid> [pid...]\n", strlen("Usage: kill [-term|-stop|-cont] <pid> [pid...]\n"));
            s_exit(1);
            return NULL;
        }
        start_idx = 2; // Skip the flag argument
    }

    // Check if we have any PIDs
    if (args[start_idx] == NULL) {  
        s_write(STDERR_FILENO, "Error: kill command requires at least one PID\n", strlen("Error: kill command requires at least one PID\n"));
        s_write(STDERR_FILENO, "Usage: kill [-term|-stop|-cont] <pid> [pid...]\n", strlen("Usage: kill [-term|-stop|-cont] <pid> [pid...]\n"));
        s_exit(1);
        return NULL;
    }

    // Process all PIDs
    for (int i = start_idx; args[i] != NULL; i++) {
        int pid = atoi(args[i]);
        s_kill(pid, signal);
    }
    s_exit(0);
    return NULL;
}


void* man(void* arg) {
    //char** command = (char**)arg;
 
    s_write(STDERR_FILENO, "Available commands:\n\n", strlen("Available commands:\n\n"));
    s_write(STDERR_FILENO, "ps          - List all running processes and their states\n", strlen("ps          - List all running processes and their states\n"));
    s_write(STDERR_FILENO, "zombify     - Create a zombie process\n", strlen("zombify     - Create a zombie process\n"));
    s_write(STDERR_FILENO, "orphanify   - Create an orphan process\n", strlen("orphanify   - Create an orphan process\n"));
    s_write(STDERR_FILENO, "busy        - Start a CPU-intensive process\n", strlen("busy        - Start a CPU-intensive process\n"));
    s_write(STDERR_FILENO, "sleep <n>   - Sleep for n ticks\n", strlen("sleep <n>   - Sleep for n ticks\n"));
    s_write(STDERR_FILENO, "nice_pid <pid> <priority>            - Change priority of process <pid> to <priority> (0-2)\n", strlen("nice_pid <pid> <priority>            - Change priority of process <pid> to <priority> (0-2)\n"));
    s_write(STDERR_FILENO, "nice <priority> <command>   - spawns a process <command> with priority <priority>\n", strlen("nice <priority> <command>   - spawns a process <command> with priority <priority>\n"));
    s_write(STDERR_FILENO, "kill -term <pid> - Terminate process <pid>\n", strlen("kill -term <pid> - Terminate process <pid>\n"));
    s_write(STDERR_FILENO, "kill -stop <pid> - Stop process <pid>\n", strlen("kill -stop <pid> - Stop process <pid>\n"));
    s_write(STDERR_FILENO, "kill -cont <pid> - Continue process <pid>\n", strlen("kill -cont <pid> - Continue process <pid>\n"));
    s_write(STDERR_FILENO, "ls          - List all files in the current directory\n", strlen("ls          - List all files in the current directory\n"));
    s_write(STDERR_FILENO, "jobs        - List all jobs\n", strlen("jobs        - List all jobs\n"));
    s_write(STDERR_FILENO, "echo <message> - Print <message> to the shell\n", strlen("echo <message> - Print <message> to the shell\n"));
    s_write(STDERR_FILENO, "touch <filename> - Create a new file with name <filename>\n", strlen("touch <filename> - Create a new file with name <filename>\n"));
    s_write(STDERR_FILENO, "rm <filename> - Delete the file <filename>\n", strlen("rm <filename> - Delete the file <filename>\n"));
    s_write(STDERR_FILENO, "cp <source> <destination> - Copy the file <source> to <destination>\n", strlen("cp <source> <destination> - Copy the file <source> to <destination>\n"));
    s_write(STDERR_FILENO, "cat <filename> - Print the contents of the file <filename>\n", strlen("cat <filename> - Print the contents of the file <filename>\n"));
    s_write(STDERR_FILENO, "chmod <mode> <filename> - Change the permissions of <filename> to <mode>\n", strlen("chmod <mode> <filename> - Change the permissions of <filename> to <mode>\n"));
    s_write(STDERR_FILENO, "mv <source> <destination> - Move the file <source> to <destination>\n", strlen("mv <source> <destination> - Move the file <source> to <destination>\n"));
    s_write(STDERR_FILENO, "man         - Show this help message\n", strlen("man         - Show this help message\n"));


    
    s_exit(0);
    return NULL;
}

void* jobs_command(void* arg) {
    print_all_jobs();
    s_exit(0);
    return NULL;
}

// TODO: make sure we s_exit() in every single case for all commands

void* ls(void* arg) {
    char** command = (char**)arg;
    s_ls(command[1]); // will be NULL if no argument is provided
    s_exit(0);
    return NULL;
}

void* echo(void* arg) {
    char** command = (char**) arg;
    int ret = s_write(STDOUT_FILENO, command[1], strlen(command[1]));
    if (ret < 0) {
        s_exit(ret);
        return NULL;
    }

    int ret2 = s_write(STDOUT_FILENO, "\n", 1);
    if (ret2 < 0) {
        s_exit(ret2);
        return NULL;
    }
    s_exit(0);
    return NULL;
}

void* touch(void* arg) {
    char** command = (char**)arg;
    if (command[1] == NULL)
    {
        char* error_message = "touch got wrong number of arguments (expected at least 1 argument)\n";
        s_write(STDERR_FILENO, error_message, strlen(error_message));
        return NULL;
    }
    for (int i = 1; command[i] != NULL; i++) {
        int fd = s_open(command[i], F_APPEND);
        if (fd < 0)
        {
            char* error_message = "touch: failed to open file\n";
            s_write(STDERR_FILENO, error_message, strlen(error_message));
            s_exit(fd);
            return NULL;
        }
        int status = s_write(fd, NULL, 0);
        if (status < 0)
        {
            char* error_message = "touch: failed to write to file\n";
            s_write(STDERR_FILENO, error_message, strlen(error_message));
            s_exit(status);
            return NULL;
        }
        s_close(fd);
    }
    s_exit(0);
    return NULL;
}

void* rm(void* arg) {
    char** command = (char**)arg;
    if (command[1] == NULL)
    {
        char* error_message = "rm got wrong number of arguments (expected at least 1 argument)\n";
        s_write(STDERR_FILENO, error_message, strlen(error_message));
        return NULL;
    }

    for (int i = 1; command[i] != NULL; i++) {
        int unlink_status = s_unlink(command[i]);
        if (unlink_status < 0)
        {
            char* error_message = "rm: Error - failed to remove file\n";
            s_write(STDERR_FILENO, error_message, strlen(error_message));
            s_exit(unlink_status);
            return NULL;
        }
    }
    s_exit(0);
    return NULL;
}

void* cp(void* arg) {
    char** command = (char**)arg;

    bool has_three_args = command[1] != NULL && command[2] != NULL && command[3] == NULL;
    if (!has_three_args)
    {
        char* error_message = "cp got wrong number of arguments (expected at  2 arguments)\n";
        s_write(STDERR_FILENO, error_message, strlen(error_message));
        return NULL;
    }

    // Open source file
    int src_fd = s_open(command[1], F_READ);
    if (src_fd < 0)
    {
        char* error_message = "cp: Error - failed to open source file\n";
        s_write(STDERR_FILENO, error_message, strlen(error_message));
        s_exit(src_fd);
        return NULL;
    }

    // Open destination file (overwrite, or create if it doesn't exist)
    int dest_fd = s_open(command[2], F_WRITE);
    if (dest_fd < 0)
    {
        char* error_message = "cp: Error - failed to create destination file\n";
        s_write(STDERR_FILENO, error_message, strlen(error_message));
        s_close(src_fd);
        s_exit(dest_fd);
        return NULL;
    }

    // Copy data
    char buffer[1024];
    int bytes_read;
    while ((bytes_read = s_read(src_fd, sizeof(buffer), buffer)) > 0)
    {
        if (s_write(dest_fd, buffer, bytes_read) < 0)
        {
            char* error_message = "cp: Error - failed to write to destination file\n";
            s_write(STDERR_FILENO, error_message, strlen(error_message));
            s_close(src_fd);
            s_close(dest_fd);
            s_exit(dest_fd);
            return NULL;
        }
    }

    s_close(src_fd);
    s_close(dest_fd);
    s_exit(0);
    return NULL;
}

void *cat(void *arg) {
    char** command = (char**)arg;

    const int buf_size = 1024;
    char buffer[buf_size];
    int bytes_read;
    if (command[1] == NULL) {
        // cat with no arguments reads from stdin and prints to stdout
        while ((bytes_read = s_read(STDIN_FILENO, buf_size, buffer)) > 0) {
            s_write(STDOUT_FILENO, buffer, bytes_read);
        }
        s_exit(0);
        return NULL;
    }

    // otherwise read from each file and print to stdout
    for (int i = 1; command[i] != NULL; i++) {
        int fd = s_open(command[i], F_READ);
        if (fd < 0) {
            char* error_message = "cat: Error - failed to open file\n";
            s_write(STDERR_FILENO, error_message, strlen(error_message));
            s_exit(fd);
            return NULL;
        }
        while ((bytes_read = s_read(fd, buf_size, buffer)) > 0) {
            s_write(STDOUT_FILENO, buffer, bytes_read);
        }
        s_close(fd);
    }
    s_exit(0);
    return NULL;
}

void* chmod(void* arg) {
    char** command = (char**)arg;
    bool has_two_args = command[1] != NULL && command[2] != NULL && command[3] == NULL;
    if (!has_two_args)
    {
        char* error_message = "chmod got wrong number of arguments (expected 2 arguments)\n";
        s_write(STDERR_FILENO, error_message, strlen(error_message));
        s_exit(-200);
        return NULL;
    }

    // Parse permission mode
    char* permission_mode = command[1];
    if (strlen(permission_mode) < 2 || strlen(permission_mode) > 4)
    {
        char* error_message = "chmod: Error - invalid permission mode (must be 2-4 characters long including + or -)\n";
        s_write(STDERR_FILENO, error_message, strlen(error_message));
        s_exit(-201);
        return NULL;
    }

    int mode;
    if (permission_mode[0] == '+') {
        mode = F_CHMOD_ADD;
    } else if (permission_mode[0] == '-') {
        mode = F_CHMOD_REMOVE;
    } else {
        char* error_message = "chmod: Error - invalid permission mode (must start with + or -)\n";
        s_write(STDERR_FILENO, error_message, strlen(error_message));
        s_exit(-202);
        return NULL;
    }

    // NOTE: we allow for repeats of the same type of permission (e.g +rr or -www or +xxr)
    uint8_t permissions = 0;
    for (int i = 1; i < strlen(permission_mode); i++) {
        if (permission_mode[i] == 'r') {
            permissions |= F_CHMOD_R;
        } else if (permission_mode[i] == 'w') {
            permissions |= F_CHMOD_W;
        } else if (permission_mode[i] == 'x') {
            permissions |= F_CHMOD_X;
        }
    }

    int chmod_status = s_chmod(command[2], permissions, mode);
    if (chmod_status < 0)
    {
        char* error_message = "chmod: Error - failed to change permissions\n";
        s_write(STDERR_FILENO, error_message, strlen(error_message));
        s_exit(chmod_status);
        return NULL;
    }
    s_exit(0);
    return NULL;
}

void* mv(void* arg) {
    char** command = (char**)arg;
    bool has_two_args = command[1] != NULL && command[2] != NULL && command[3] == NULL;
    if (!has_two_args) {
        char* error_message = "mv got wrong number of arguments (expected 2 arguments)\n";
        s_write(STDERR_FILENO, error_message, strlen(error_message));
        s_exit(-200);
        return NULL;
    }
    int mv_status = s_mv(command[1], command[2]);
    if (mv_status < 0) {
        char* error_message = "mv: Error - failed to move file\n";
        s_write(STDERR_FILENO, error_message, strlen(error_message));
        s_exit(mv_status);
        return NULL;
    }
    s_exit(0);
    return NULL;
}

void* hang_helper(void* arg) {
    s_exit(0);
    return NULL;
}

void* logout(void* arg) {
    s_logout();
    s_exit(0);
    return NULL;
}


void* execute_command(void* arg) {
    char** ctx = (char**)arg;
    
    // We always want the first command to be the command name
    if (ctx == NULL || ctx[0] == NULL) {
        return NULL;
    }
    if (strcmp(ctx[0], "ps") == 0) {
        ps(ctx);
        printf("ps was called and finished\n");
        return NULL;
    }
    if (strcmp(ctx[0], "zombify") == 0) {
        zombify(ctx);
        return NULL;
    }
    if (strcmp(ctx[0], "orphanify") == 0) {
        return orphanify(ctx);
    }
    if (strcmp(ctx[0], "ls") == 0) {
        return ls(ctx);
    }
    if (strcmp(ctx[0], "echo") == 0) {
        return echo(ctx);
    }
    if (strcmp(ctx[0], "touch") == 0) {
        return touch(ctx);
    }
    if (strcmp(ctx[0], "cp") == 0) {
        return cp(ctx);
    }
    if (strcmp(ctx[0], "rm") == 0) {
        return rm(ctx);
    }
    if (strcmp(ctx[0], "chmod") == 0) {
        return chmod(ctx);
    }
    if (strcmp(ctx[0], "cat") == 0) {
        return cat(ctx);
    }
    if (strcmp(ctx[0], "mv") == 0) {
        return mv(ctx);
    }
    if (strcmp(ctx[0], "busy") == 0) {
        char* priority_level = ctx[1] == NULL ? "1" : ctx[1];
        return busy(ctx, priority_level);
    }
    if (strcmp(ctx[0], "sleep") == 0) {
        return sleep_command(ctx, ctx[1]);
    }
    if (strcmp(ctx[0], "nice_pid") == 0) {
        return nice_pid_command(ctx, ctx[1], ctx[2]);
    }
    if (strcmp(ctx[0], "nice") == 0) {
        return nice_command(ctx, ctx[1]);
    }
    if (strcmp(ctx[0], "man") == 0) {
        return man(ctx);
    }
    if (strcmp(ctx[0], "kill") == 0) {
        return kill_process_shell(ctx, ctx[1]);
    }
    if (strcmp(ctx[0], "jobs") == 0) {
        return jobs_command(ctx);
    }
    if (strcmp(ctx[0], "hang") == 0) {
        return hang(ctx);
    }
    if (strcmp(ctx[0], "nohang") == 0) {
        return nohang(ctx);
    }
    if (strcmp(ctx[0], "recur") == 0) {
        return recur(ctx);
    }
    if (strcmp(ctx[0], "crash") == 0) {
        return crash(ctx);
    }
    if (strcmp(ctx[0], "logout") == 0) {
        return logout(ctx);
    }
    s_exit(0);
    return NULL;
}
