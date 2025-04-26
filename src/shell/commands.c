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

#define BUFFER_SIZE 256
#define BUSY_LOOP_ITERATIONS 100000000

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
    
    s_get_process_info();
    printf("ps called\n");
    s_exit(0);
    return NULL;
}
void* zombie_child(void* arg) {
    // Child process exits normally
    LOG_INFO("Child process running, will exit soon");
    s_exit(0);
    return NULL;
}


void* zombify(void* arg) {
    
    // Spawn the child process
    pid_t child = s_spawn(zombie_child, (char*[]){"zombie_child", NULL}, STDIN_FILENO, STDOUT_FILENO);
    s_get_process_info();
    LOG_INFO("Spawned child process with PID %d", child);
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
    s_spawn(orphan_child, (char*[]){"orphan_child", NULL}, STDIN_FILENO, STDOUT_FILENO);
    s_exit(0);
    return NULL;
}


// /**
//  * @brief Busy wait indefinitely.
//  * It can only be interrupted via signals.
//  *
//  * Example Usage: busy
//  */
// void* busy(void* arg) {
//     LOG_INFO("Starting busy process");
//     struct command_context* ctx = (struct command_context*)arg;
    
//     // Set up signal handling for SIGINT
//     struct sigaction sig_action;
//     sig_action.sa_handler = busy_sigint_handler;  // Use our custom handler
//     sigemptyset(&sig_action.sa_mask);
//     sig_action.sa_flags = 0;
    
//     // Install signal handler for SIGINT (Ctrl-C)
//     sigaction(SIGINT, &sig_action, NULL);
    
//     // Reset the global flag
//     busy_running = 1;
    
//     // Check if a priority level was specified
//     if (ctx->command[1] != NULL) {
//         int priority_level = atoi(ctx->command[1]);

//         printf("priority_level: %d\n", priority_level);
        
//         // Update the priority of the current process
//         pcb_t* proc = scheduler_state->curr;
        
//         // Remove from current priority queue
//         if (proc->priority == PRIORITY_HIGH) {
//             linked_list_remove(&scheduler_state->priority_high, proc, priority_pointers.prev, priority_pointers.next);
//         } else if (proc->priority == PRIORITY_MEDIUM) {
//             linked_list_remove(&scheduler_state->priority_medium, proc, priority_pointers.prev, priority_pointers.next);
//         } else {
//             linked_list_remove(&scheduler_state->priority_low, proc, priority_pointers.prev, priority_pointers.next);
//         }
        
//         // Update priority based on requested level
//         if (priority_level == 0) {
//             proc->priority = PRIORITY_HIGH;
//         } else if (priority_level == 1) {
//             proc->priority = PRIORITY_MEDIUM;
//         } else {
//             proc->priority = PRIORITY_LOW;
//         }
        
//         // Add back to the appropriate queue
//         add_process_to_queue(proc);
        
//         LOG_INFO("Set busy process priority to %d", priority_level);
//     }
    
//     // Create a CPU intensive workload
//     while(busy_running) {
//         // This is a tight loop that consumes CPU
//         for(int i = 0; i < BUSY_LOOP_ITERATIONS && busy_running; i++) {
//             // Do nothing, just burn CPU cycles
//         }
//     }
//     return NULL;
// }

// Implementation of nice command to use the system s_nice function
void* nice_command(void* arg) {
    struct command_context* ctx = (struct command_context*)arg;
    int stdout_fd = ctx->stdout_fd;
    
    if (ctx->command[1] == NULL || ctx->command[2] == NULL) {
        dprintf(stdout_fd, "Usage: nice <pid> <priority_level>\n");
        return NULL;
    }
    
    // Parse PID and priority level
    pid_t target_pid = atoi(ctx->command[1]);
    int priority_level = atoi(ctx->command[2]);
    
    // Validate priority level
    if (priority_level < 0 || priority_level > 2) {
        dprintf(stdout_fd, "Invalid priority level. Use 0 (high), 1 (medium), or 2 (low)\n");
        return NULL;
    }
    
    // Call the system nice function
    int result = s_nice(target_pid, priority_level);
    
    if (result == 0) {
        dprintf(stdout_fd, "Changed priority of process %d to %d\n", target_pid, priority_level);
    } else {
        dprintf(stdout_fd, "Failed to change priority of process %d\n", target_pid);
    }
    
    return NULL;
}

// TODO: make sure we s_exit() in every single case for all commands

void* ls(void* arg) {
    struct command_context* ctx = (struct command_context*)arg;
    s_ls(ctx->command[1]); // will be NULL if no argument is provided
    s_exit(0);
    return NULL;
}

void* echo(void* arg) {
    char** command = (char**) arg;
    int ret = s_write(STDOUT_FILENO, command[1], strlen(command[1]));
    s_exit(ret);
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
    s_exit(0);
    return NULL;
}
