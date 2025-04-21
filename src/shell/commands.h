#ifndef CONTEXT_H
#define CONTEXT_H
#include "../scheduler/scheduler.h"

struct command_context {
    char** command;       // The command and its args
    int stdin_fd;        // Input file descriptor
    int stdout_fd;       // Output file descriptor
    pcb_t* process;
};

void* execute_command(void* arg);



#endif // CONTEXT_H
