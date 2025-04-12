#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "commands.h"
#include "../scheduler/scheduler.h"
#include "../scheduler/logger.h"
#include "../../lib/exiting_alloc.h"
#include "../scheduler/spthread.h"
#include "../scheduler/sys.h"

#define BUFFER_SIZE 256

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
    return NULL;
}
