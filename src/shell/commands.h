#ifndef CONTEXT_H
#define CONTEXT_H

#include "../scheduler/scheduler.h"
// Include sys.h in the implementation file instead to avoid redundant declarations

struct command_context {
    char** command;       // The command and its args
    int stdin_fd;        // Input file descriptor
    int stdout_fd;       // Output file descriptor
    int next_input_fd;   // Next command's input fd (for pipelines)
    pcb_t* process;
};

// Command execution functions
void* execute_command(void* arg);
void* ps(void* arg);
void* zombify(void* arg);
void* orphanify(void* arg);
void* busy(void* arg);
void* nice_command(void* arg);

#endif // CONTEXT_H
