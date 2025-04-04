#ifndef PENNSHELL_PORCELAIN_H
#define PENNSHELL_PORCELAIN_H

#include "./parser.h"

/*
 * @brief Read and parse a command from stdin.
 * this array should be freed by the caller after use.
 * @param parsed_command a double pointer to a parsed_command struct. If the command was empty or there was an error reading the command, the pointer will be NULL.
 * The caller should free this struct after use (if the pointer is not NULL).
 * @returns 0 if the command executed successfully (including if the command read was empty), 1 if there was an error.
 */
int read_command(struct parsed_command **parsed_command);

/*
 * @brief Write the prompt to STDERR
 */
void display_prompt();

/*
 * @brief Write the catchphrase to STDERR.
 */
void display_catchphrase();


#endif // PENNSHELL_PORCELAIN_H
