#ifndef CONTEXT_H
#define CONTEXT_H
#include "../scheduler/scheduler.h"

struct command_context {
    char** command;       // The command and its args
    int stdin_fd;        // Input file descriptor
    int stdout_fd;       // Output file descriptor
    int next_input_fd;   // Next command's input fd (for pipelines)
    pcb_t* process;
};

void* execute_command(void* arg);



#endif // CONTEXT_H
