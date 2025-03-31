#include "./shell_porcelain.h"
#include <assert.h>
#include <signal.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include "./exiting_alloc.h"
#include "./exiting_signal.h"
#include "./print.h"
#include "./command_execution.h"
#include "./parser.h"

#ifndef PROMPT
#define PROMPT "penn-shell> "
#endif

#ifndef CATCHPHRASE
#define CATCHPHRASE "Welcome to Penn Shell!"
#endif

enum
{
    MAX_LINE_LENGTH = 4096
};

// NOTE: this code is not thread-safe because we use global variables.
static char cmd_buffer[MAX_LINE_LENGTH + 1]; // +1 for the null terminator

int read_command(struct parsed_command **parsed_command)
{
    unsigned int num_bytes = read(STDIN_FILENO, cmd_buffer, MAX_LINE_LENGTH);
    if (num_bytes == -1)
    {
        perror("Failed to read command from stdin");
        // TODO: do we need any other cleanup here?
        exit(EXIT_FAILURE);
    }
    else if (num_bytes == 0)
    {
        // EOF
        print_stderr("\n");
        *parsed_command = NULL;
        return -1;
    }

    if (cmd_buffer[num_bytes - 1] != '\n')
    {
        print_stderr("\n");
    }
    cmd_buffer[num_bytes] = '\0';
    // cmd_buffer is now a null-terminated string that uses num_bytes + 1 bytes
    // (including the null terminator)

    int err_code = parse_command(cmd_buffer, parsed_command);
    if (err_code > 0)
    {
        perror("invalid");
        return 1;
    }
    if (err_code == -1)
    {
        perror("Failed to parse command due to system call error");
        *parsed_command = NULL;
        return 1;
    }
    return 0;
}

void display_prompt()
{
    print_stderr(PROMPT);
}


