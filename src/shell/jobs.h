#ifndef JOBS_H_
#define JOBS_H_
#include "./Job.h"
#include <stdlib.h>
#include <stdbool.h>

// Node for job linked list
typedef struct job_ll_node_st {
    job* job;
    struct job_ll_node_st* next;
    struct job_ll_node_st* prev;
} job_ll_node;

/**
 * @brief Check if `cmd` matches any of `bg`, `fg`, `jobs`, and if so
 * handle it
 * @return A boolean that indicates whether the cmd matched bg, fg, or jobs
 * and therefore if this function actually handled the passed command.
 *
 * It is generally expected that if this function returns true, 
 * the caller will not have to do further work on the given `cmd`.
 */
bool handle_jobs_commands(struct parsed_command* cmd);

void handle_jobs();

/**
 * @brief Adds a (background/stopped) job to the queue.
 *
 * Note that if the job is not background/stopped, this function
 * will throw an error.
 */
void enqueue_job(job* job);

void print_all_jobs();

// Find a job by ID
job* find_job_by_id(jid_t id);

// Continue a stopped job in the foreground
void continue_job(job* job);

// Find a job by its process ID
job* find_job_by_pid(pid_t pid);

void print_job_list();

void print_job_command(job* job);

void remove_job_by_pid(pid_t pid);

void add_foreground_job(job* job);

void remove_foreground_job(job* job);

job* get_jobs_head();

#endif // JOBS_H_
