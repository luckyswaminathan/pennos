#include "./shell_porcelain.h"
#include <assert.h>
#include <signal.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include "src/utils/errno.h"
#include "../../lib/exiting_alloc.h"
#include "./exiting_signal.h"
#include "./command_execution.h"
#include "./parser.h"
#include "src/scheduler/sys.h"

#ifndef PROMPT
#define PROMPT "$ "
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
    unsigned int num_bytes = s_read(STDIN_FILENO, MAX_LINE_LENGTH, cmd_buffer);
    if (num_bytes == -1)
    {
        u_perror("Failed to read command from stdin");
        // TODO: do we need any other cleanup here?
        exit(EXIT_FAILURE);
    }
    else if (num_bytes == 0)
    {
        // EOF
        s_write(STDERR_FILENO, "\n", 1);
        *parsed_command = NULL;
        return -1;
    }

    if (cmd_buffer[num_bytes - 1] != '\n')
    {
        s_write(STDERR_FILENO, "\n", 1);
    }
    cmd_buffer[num_bytes] = '\0';
    // cmd_buffer is now a null-terminated string that uses num_bytes + 1 bytes
    // (including the null terminator)

    int err_code = parse_command(cmd_buffer, parsed_command);
    if (err_code > 0)
    {
        char* err_msg = "Unparseable command\n";
        s_write(STDERR_FILENO, err_msg, strlen(err_msg));
        return 1;
    }
    if (err_code == -1)
    {
        char* err_msg = "Failed to parse command due to system call error\n";
        s_write(STDERR_FILENO, err_msg, strlen(err_msg));
        *parsed_command = NULL;
        return 1;
    }
    return 0;
}

void display_prompt()
{
    s_write(STDERR_FILENO, PROMPT, strlen(PROMPT));
}


