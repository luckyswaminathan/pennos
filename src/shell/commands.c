#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "commands.h"
#include "../scheduler/scheduler.h"
#include "../scheduler/logger.h"
#include "../scheduler/sys.h"
#include "../../lib/exiting_alloc.h"
#include "../scheduler/spthread.h"

#define BUFFER_SIZE 256

// CPU usage constants
#define BUSY_LOOP_ITERATIONS 10000000
#define TOTAL_TIME_SLICES 18.0
#define HIGH_PRIORITY_SLICES 9.0
#define MEDIUM_PRIORITY_SLICES 6.0
#define LOW_PRIORITY_SLICES 3.0

static void print_header(int output_fd) {
    const char* header = "PID PPID PRI STAT CMD\n";
    dprintf(output_fd, "%s", header);
    LOG_INFO("%s", header);
}
static void print_process(pcb_t* proc, int output_fd) {
    char state_char = 'R';
    if (proc->state == PROCESS_BLOCKED){ state_char = 'B';
    } else if (proc->state == PROCESS_ZOMBIED) {
        state_char = 'Z';
    }
    dprintf(output_fd, "%3d %4d %3d %c   %s\n", proc->pid, proc->ppid, proc->priority, state_char, proc->command);
    LOG_INFO("Process info - PID: %d, PPID: %d, STATE: %d, COMMAND: %s", proc->pid, proc->ppid, proc->state, proc->command);
}



// Implementation of ps command
void* ps(void* arg) {
    LOG_INFO("ps command executed");
    struct command_context* ctx = (struct command_context*)arg;
    int stdout_fd = ctx->stdout_fd;
    
    // Iterate through high priority queue
    pcb_t* proc = scheduler_state->priority_high.head;
    print_header(stdout_fd);
    while (proc != NULL) {
        print_process(proc, stdout_fd);
        proc = proc->priority_pointers.next;
    }
    
    // Iterate through medium priority queue
    proc = scheduler_state->priority_medium.head;
    while (proc != NULL) {
        print_process(proc, stdout_fd);
        proc = proc->priority_pointers.next;
    }
    
    // Iterate through low priority queue
    proc = scheduler_state->priority_low.head;
    while (proc != NULL) {
        print_process(proc, stdout_fd);
        proc = proc->priority_pointers.next;
    }
    
    // Iterate through sleeping queue
    proc = scheduler_state->sleeping_processes.head;
    while (proc != NULL) {
        print_process(proc, stdout_fd);
        proc = proc->priority_pointers.next;
    }
    
    // Iterate through terminated queue
    proc = scheduler_state->terminated_processes.head;
    while (proc != NULL) {
        LOG_INFO("Terminated process info - PID: %d, PPID: %d, STATE: %d, COMMAND: %s", proc->pid, proc->ppid, proc->state, proc->command);
        print_process(proc, stdout_fd);
        proc = proc->priority_pointers.next;
    }
    
    return NULL;
}
void* zombie_child(void* arg) {
    // Child process exits normally
    LOG_INFO("Child process running, will exit soon");
    return NULL;
}

void* zombify(void* arg) {
    
    struct command_context* child_ctx = exiting_malloc(sizeof(struct command_context));
    child_ctx->command = exiting_malloc(sizeof(char*));
    child_ctx->command[0] = strdup("zombie_child");
    child_ctx->process = NULL;
    // Spawn the child process
    pid_t child = s_spawn(zombie_child, child_ctx);
    LOG_INFO("Spawned child process with PID %d", child);
    while(1) {  
    };
    return NULL;
}

void* orphan_child(void* arg) {
    
    while(1) {
    };
    return NULL;
}

void* orphanify(void* arg) {
    struct command_context* child_ctx = exiting_malloc(sizeof(struct command_context));
    child_ctx->command = exiting_malloc(sizeof(char*));
    child_ctx->command[0] = strdup("orphan_child");
    child_ctx->process = NULL;
    s_spawn(orphan_child, child_ctx);
    return NULL;
    
}

void* busy(void* arg) {
    LOG_INFO("Starting busy process");
    struct command_context* ctx = (struct command_context*)arg;
    
    // Check if a priority level was specified
    if (ctx->command[1] != NULL) {
        int priority_level = atoi(ctx->command[1]);
        
        // Update the priority of the current process
        pcb_t* proc = scheduler_state->curr;
        
        // Remove from current priority queue
        if (proc->priority == PRIORITY_HIGH) {
            linked_list_remove(&scheduler_state->priority_high, proc, priority_pointers.prev, priority_pointers.next);
        } else if (proc->priority == PRIORITY_MEDIUM) {
            linked_list_remove(&scheduler_state->priority_medium, proc, priority_pointers.prev, priority_pointers.next);
        } else {
            linked_list_remove(&scheduler_state->priority_low, proc, priority_pointers.prev, priority_pointers.next);
        }
        
        // Update priority based on requested level
        if (priority_level == 0) {
            proc->priority = PRIORITY_HIGH;
        } else if (priority_level == 1) {
            proc->priority = PRIORITY_MEDIUM;
        } else {
            proc->priority = PRIORITY_LOW;
        }
        
        // Add back to the appropriate queue
        add_process_to_queue(proc);
        
        LOG_INFO("Set busy process priority to %d", priority_level);
    }
    
    // Create a CPU intensive workload
    while(1) {
        // This is a tight loop that consumes CPU
        for(int i = 0; i < BUSY_LOOP_ITERATIONS; i++) {
            // Do nothing, just burn CPU cycles
        }
    }
    return NULL;
}

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

void* execute_command(void* arg) {
    struct command_context* ctx = (struct command_context*)arg;
    if (ctx->command == NULL) {
        return NULL;
    }
    if (strcmp(ctx->command[0], "ps") == 0) {
        return ps(ctx);
    }
    if (strcmp(ctx->command[0], "zombify") == 0) {
        return zombify(ctx);
    }
    if (strcmp(ctx->command[0], "orphanify") == 0) {
        return orphanify(ctx);
    }
    if (strcmp(ctx->command[0], "busy") == 0) {
        return busy(ctx);
    }
    if (strcmp(ctx->command[0], "nice") == 0) {
        return nice_command(ctx);
    }
    return NULL;
}
