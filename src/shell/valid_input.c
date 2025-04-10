#include "./command_execution.h"
#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>


void validate_command(struct parsed_command *parsed_command) {
    bool is_pipe = false;
    bool is_redirect = false;

    size_t arglen = parsed_command->num_commands;
    
    for (size_t i = 0; i < arglen; i++) {
        char *cmd = parsed_command->commands[i][0];
        
        if (strcmp(cmd, "|") == 0) {
            is_pipe = true;
        } else if (strcmp(cmd, ">") == 0 || strcmp(cmd, "<") == 0) {
            is_redirect = true;
        }
    }

    if (is_pipe && is_redirect) {
        perror("invalid");
    }
}
