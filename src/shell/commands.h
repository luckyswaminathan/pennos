#ifndef CONTEXT_H
#define CONTEXT_H

struct command_context {
    char** command;       // The command and its args
    int stdin_fd;        // Input file descriptor
    int stdout_fd;       // Output file descriptor
};

void* ps(void* arg);

#endif // CONTEXT_H
