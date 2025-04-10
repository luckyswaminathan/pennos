#ifndef JOB_H_
#define JOB_H_

#include <stdint.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "./parser.h"

// define new type "job id"
typedef uint64_t jid_t;

typedef enum job_status_enum {
  J_RUNNING_FG = 0,
  J_RUNNING_BG = 1,
  J_STOPPED    = 2
} job_status;

// Represents a job
typedef struct job_st {
  jid_t id;
  pid_t* pids;        // Array of process IDs in the job
  size_t num_processes;  // Number of processes in the job
  job_status status;
  struct parsed_command* cmd;
} job;

/**
 * @brief print a representation of a job
 */
void print_job(job* job);

/**
 * @brief Frees the job, also destroying its fields as needed (ie, the `cmd` field). 
 * Note that it is assumed that the job contains the only reference to the fields within
 * it (specifically, to the `cmd` field of type `parsed_command*`), thus this function
 * will free the fields in the job struct as necessary.
 */
void destroy_job(job* job);

#endif  // JOB_H_
