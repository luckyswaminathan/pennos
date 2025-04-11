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
    LOG_INFO("Process info - PID: %d, PPID: %d, STATE: %d", proc->pid, proc->ppid, proc->state);
}

// Print header for ps output
static void print_header(int output_fd) {
    const char* header = "PID PPID STATE PRIORITY COMMAND\n";
    LOG_INFO("%s", header);
}

// Implementation of ps command
void* ps(void* arg) {
    struct command_context* ctx = (struct command_context*)arg;
    int stdout_fd = ctx->stdout_fd;
    // Print header
    print_header(stdout_fd);
    
    pcb_t* proc = scheduler_state->processes.head;
    while (proc != NULL) {
        print_process(proc, stdout_fd);
        proc = proc->process_pointers.next;
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
