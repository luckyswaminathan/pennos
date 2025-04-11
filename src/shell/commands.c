#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "commands.h"
#include "../scheduler/scheduler.h"
#include "../scheduler/logger.h"

#define BUFFER_SIZE 256

// Print information about a single process
static void print_process(pcb_t* proc, int output_fd) {
    char state_char = 'R';
    if (proc->state == PROCESS_BLOCKED){ state_char = 'B';
    } else if (proc->state == PROCESS_TERMINATED) {
        state_char = 'T';
    }
    dprintf(output_fd, "%3d %4d %3d %c   %s\n", proc->pid, proc->ppid, proc->priority, state_char, proc->command);
    LOG_INFO("Process info - PID: %d, PPID: %d, STATE: %d, COMMAND: %s", proc->pid, proc->ppid, proc->state, proc->command);
}

// Print header for ps output
static void print_header(int output_fd) {
    const char* header = "PID PPID PRI STAT CMD\n";
    dprintf(output_fd, "%s", header);
    LOG_INFO("%s", header);
}

// Implementation of ps command
void* ps(void* arg) {
    struct command_context* ctx = (struct command_context*)arg;
    int stdout_fd = ctx->stdout_fd;
    // Print header
    print_header(stdout_fd);
    
    // Iterate through high priority queue
    pcb_t* proc = scheduler_state->priority_high.head;
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
        print_process(proc, stdout_fd);
        proc = proc->priority_pointers.next;
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
    return NULL;
}
