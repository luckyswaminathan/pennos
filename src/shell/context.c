#include "context.h"
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

void* execute_command(struct command_context* context) {
    // Set up stdin redirection if needed
    if (context->stdin_fd != STDIN_FILENO && 
        dup2(context->stdin_fd, STDIN_FILENO) == -1) {
        perror("Failed to redirect stdin");
        return (void*)EXIT_FAILURE;
    }
    
    // Set up stdout redirection if needed
    if (context->stdout_fd != STDOUT_FILENO && 
        dup2(context->stdout_fd, STDOUT_FILENO) == -1) {
        perror("Failed to redirect stdout");
        return (void*)EXIT_FAILURE;
    }

    // Close the next command's input fd if it exists
    if (context->next_input_fd != -1) {
        close(context->next_input_fd);
    }

    // Execute the command
    execvp(context->command[0], context->command);
    
    // If we get here, execvp failed
    perror("Failed to execve command");
    return (void*)EXIT_FAILURE;
}

