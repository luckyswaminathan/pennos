#ifndef CONTEXT_H
#define CONTEXT_H

struct command_context {
    char** command;       // The command and its args
    int stdin_fd;        // Input file descriptor
    int stdout_fd;       // Output file descriptor
};

// Command execution functions
void* execute_command(void* arg);
void* ps(void* arg);
void* zombify(void* arg);
void* orphanify(void* arg);
void* busy(void* arg);
void* nice_command(void* arg);

#endif // CONTEXT_H
