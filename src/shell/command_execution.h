#ifndef COMMAND_EXECUTION_H_
#define COMMAND_EXECUTION_H_

#include "./parser.h"
#include "./Job.h"

/**
 * @brief A higher level function that manages the execution of the stages of a command
 *
 * This function expects that it is only called if the caller thinks there is something to execute (not checking
 * for cases like the num_commands being 0).
 */
void execute_job(job* job);

extern pid_t current_pid;

// Execute the lead child process
void execute_job_lead_child(job* job, struct parsed_command* parsed_command);

#endif // COMMAND_EXECUTION_H_
