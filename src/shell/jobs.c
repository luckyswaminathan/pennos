#include "./Job.h"
#include "./jobs.h"
#include "../../lib/linked_list.h"
#include "../../lib/exiting_alloc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "./command_execution.h"
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>  // For kill() and SIGCONT
#include "../scheduler/sys.h"
#include "../scheduler/logger.h"

typedef linked_list(job_ll_node) job_ll;

/**
 * @brief The global linked list storing background and stopped jobs.
 */
static job_ll _jobs = {
  .head = NULL,
  .tail = NULL,
  .ele_dtor = NULL
};
/**
 * @brief Pointer to the global jobs list.
 */
static job_ll* jobs = &_jobs;

/**
 * @brief Prints the command line representation of a job to stderr.
 *
 * Iterates through the parsed command structure within the job
 * and prints each command and argument, separated by pipes if necessary.
 *
 * @param job A pointer to the job whose command should be printed.
 */
void print_job_command(job* job) {
  for (size_t i = 0; i < job->cmd->num_commands; i++) {
    if (i > 0) {
      fprintf(stderr, " | ");
    }

    char **command = job->cmd->commands[i];
    for (size_t j = 0; command[j] != NULL; j++) {
      if (j > 0) {
        fprintf(stderr, " ");
      }
      fprintf(stderr, "%s", command[j]);
    }
  }
  fprintf(stderr, "\n");
}

/**
 * @brief Handles the "jobs" built-in command.
 *
 * Iterates through the global jobs list and prints information
 * (job ID and command) for each job to stderr using s_fprintf_short.
 */
void handle_jobs() {
  job_ll_node* node = linked_list_head(jobs);

  while (node != NULL) {
    // s_fprintf_short(STDERR_FILENO, "[%lu] ", node->job->id);
    print_job_command(node->job);
    // s_fprintf_short(STDERR_FILENO, "\n");
    node = linked_list_next(node);
  }
}

/**
 * @brief Prints all jobs in the list along with their first PID to stderr.
 *
 * Note: This function seems primarily for debugging and uses fprintf directly.
 */
void print_all_jobs() {
  job_ll_node* node = linked_list_head(jobs);
  while (node != NULL) {
    fprintf(stderr, "[%lu] ", node->job->id);
    fprintf(stderr, "%d", node->job->pid);
    fprintf(stderr, "\n");
    node = linked_list_next(node);
  }
}


/**
 * @brief Handles the "fg" built-in command.
 *
 * Brings the specified job (by ID) or the most recently added job
 * from the background/stopped list to the foreground.
 * It removes the job from the list, gives it terminal control,
 * sends SIGCONT if stopped, and waits for it.
 *
 * @param cmd The parsed command structure, potentially containing the job ID.
 */
void handle_fg(struct parsed_command* cmd) {
  char* target_id_str = cmd->commands[0][1];
  job_ll_node* node = linked_list_tail(jobs);

  if (target_id_str != NULL) {
    // search for the target id
    jid_t target_id = strtol(target_id_str, NULL, 10);

    while (node != NULL) {
      if (node->job->id == target_id) {
        break;
      }
      node = linked_list_prev(node);
    }

    // Check if we found the job
    if (node == NULL) {
      fprintf(stderr, "No job with id %ld\n", target_id);
      return;
    }
  } else {
    // node is already the tail of the queue
    // so we have LIFO
    
    if (node == NULL) {
      fprintf(stderr, "No jobs to fg\n");
      return;
    }
  }

  // remove it from the linked list
  linked_list_remove(jobs, node);
  job* job = node->job;
  free(node); // free the node (but not the job)
              // TODO: we should make use of the ele_dtor here

              // mark the job as running in the foreground
  
  print_job_command(job);
  fprintf(stderr, "\n");

  if (job->status == J_STOPPED) {
    s_kill(job->pid, P_SIGCONT);
  }

  int status;
  // mark the job as running
  job->status = J_RUNNING_FG;
  s_waitpid(job->pid, &status, false);
  // NOTE: this is identical logic to command_execution.c
  if (P_WIFSTOPPED(status)) {
      job->status = J_STOPPED;
      remove_foreground_job(job);
      enqueue_job(job);
  }
}

/**
 * @brief Handles the "bg" built-in command.
 *
 * Resumes the specified stopped job (by ID) or the most recently stopped job
 * in the background.
 * It finds the job, changes its status to J_RUNNING_BG, and sends SIGCONT.
 * The job remains in the jobs list.
 *
 * @param cmd The parsed command structure, potentially containing the job ID.
 */
void handle_bg(struct parsed_command* cmd) {
  job_ll_node* node = linked_list_tail(jobs);
  char* target_id_str = cmd->commands[0][1];

  if (target_id_str != NULL) {
    jid_t target_id = strtol(target_id_str, NULL, 10);

    while (node != NULL) {
      if (node->job->id == target_id) {
        break;
      }
      node = linked_list_prev(node);
    }

    // Check if we found the job
    if (node == NULL) {
      fprintf(stderr, "No job with id %ld\n", target_id);
      return;
    }
  } else {
    // node is already the tail of the queue
    // LIFO!

    if (node == NULL) {
      fprintf(stderr, "No jobs to bg\n");
      return;
    }
  }
    
  job* job = node->job;
  if (job->status == J_RUNNING_FG || job->status == J_RUNNING_BG) {
    fprintf(stderr, "Job %ld is already running in foreground or background\n", job->id);
    return;
  }

  fprintf(stderr, "Resuming job %d\n", job->pid);

  job->status = J_RUNNING_BG;

  fprintf(stderr, "Status changed\n");

  // Resume the job in the background
  s_kill(job->pid, P_SIGCONT);

  fprintf(stderr, "Running: ");
  print_job_command(job);
  fprintf(stderr, "\n");
}

/**
 * @brief Determines if a parsed command is a job control built-in (jobs, fg, bg)
 *        and handles it if it is.
 *
 * @param cmd The parsed command structure.
 * @return true if the command was a job control built-in and was handled, false otherwise.
 */
bool handle_jobs_commands(struct parsed_command* cmd) {
  if (cmd->num_commands != 1) {
    return false;
  }

  char* command = cmd->commands[0][0];

  if (strcmp(command, "jobs") == 0) {
    handle_jobs();
    return true;
  }
  if (strcmp(command, "fg") == 0) {
    handle_fg(cmd);
    return true;
  }

  if (strcmp(command, "bg") == 0) {
    handle_bg(cmd);
    return true;
  }

  return false;
}

/**
 * @brief Adds a job to the end of the background/stopped jobs list.
 *
 * This function is typically called for jobs launched in the background
 * or for foreground jobs that have been stopped.
 * It performs error checking to ensure only background or stopped jobs are enqueued.
 *
 * @param job The job to add to the list.
 */
void enqueue_job(job* job) {
  if (job->status != J_STOPPED && job->status != J_RUNNING_BG) {
    fprintf(stderr, "Cannot enqueue a job that is not stopped or running in the background (cannot enqueue foreground jobs).\n");
    exit(EXIT_FAILURE);
  }

  job_ll_node* node = (job_ll_node*) exiting_malloc(sizeof(job_ll_node));
  node->prev = NULL;
  node->next = NULL;
  node->job = job;
  linked_list_push_tail(jobs, node);
  // print_all_jobs();
  // print_job_list();
}

/**
 * @brief Finds a job in the list by its job ID.
 *
 * @param id The job ID to search for.
 * @return A pointer to the job if found, NULL otherwise.
 */
job* find_job_by_id(jid_t id) {
  job_ll_node* node = linked_list_head(jobs);

  while (node) {
    if (node->job->id == id) {
      return node->job;
    }
    node = linked_list_next(node);
  }

  return NULL;
}

/**
 * @brief Sets a job's status to J_RUNNING_FG and executes it.
 *
 * Note: This function seems intended to continue a stopped job in the foreground,
 * but might need integration with terminal control transfer.
 *
 * @param job The job to continue.
 */
void continue_job(job* job) {
  job->status = J_RUNNING_FG;
  execute_job(job);
}

/**
 * @brief Finds a job in the list by its process group ID (or first PID).
 *
 * Iterates through the list, comparing the given PID with the first PID
 * stored in each job.
 *
 * @param pid The process ID (expected to be the process group ID) to search for.
 * @return A pointer to the job if found, NULL otherwise.
 */
job* find_job_by_pid(pid_t pid) {
  job_ll_node* node = linked_list_head(jobs);

  while (node) {
    if (node->job->pid > 0) {
      if (node->job->pid == pid) {
        return node->job;
      }
    }
    node = linked_list_next(node);
  }

  return NULL;
}

/**
 * @brief Prints the command for every job in the list.
 *
 * Note: Marked with TODO. Its intended use compared to handle_jobs() is unclear.
 * @todo Clarify the purpose of this function.
 */
// TODO: what is this used for
void print_job_list() {
  job_ll_node* node = linked_list_head(jobs);
  while (node) {
    print_job_command(node->job);
    node = linked_list_next(node);
  }
}

/**
 * @brief Removes a job from the list based on its process group ID (or first PID)
 *        and cleans up its resources.
 *
 * Finds the job node by PID, removes it from the linked list, prints a completion
 * message, destroys the job struct, and frees the list node.
 *
 * @param pid The process ID (expected to be the process group ID) of the job to remove.
 */
void remove_job_by_pid(pid_t pid) {
  job_ll_node* node = linked_list_head(jobs);

  while (node != NULL) {
    if (node->job->pid == pid) {
      break; // we've found the node with the PID
    }
    node = linked_list_next(node);
  }

  if (node != NULL) {
    linked_list_remove(jobs, node);

    // TODO: Update to 
    print_parsed_command(node->job->cmd);

    // Free the job resources
    destroy_job(node->job);
    free(node);
  } else {
    // Job not found - this is expected for foreground jobs that were already cleaned up
  }
}

/**
 * @brief Adds a job node representing the current foreground job to the head of the list.
 *
 * This seems to be used to temporarily track the foreground job, potentially
 * for signal handling purposes or easy access via get_jobs_head().
 *
 * @param job The job currently running in the foreground.
 */
void add_foreground_job(job* job) {
  job_ll_node* node = (job_ll_node*) exiting_malloc(sizeof(job_ll_node));
  node->prev = NULL;
  node->next = NULL;
  node->job = job;
  linked_list_push_head(jobs, node);
}

/**
 * @brief Removes a specific job pointer from the jobs list.
 *
 * Searches the list for a node containing the exact job pointer provided
 * and removes that node.
 * Note: The comment suggests this is used for removing stopped jobs,
 * implying a potential misnomer.
 *
 * @param job The pointer to the job struct whose corresponding node should be removed.
 * @todo Rename this function to better reflect its actual usage (removing specific stopped jobs?).
 */
// Deceptively, this doesn't only remove foreground jobs. In fact, we only use it to remove jobs that were recently set to have J_STOPPED status
// TODO: rename this
void remove_foreground_job(job* job) {
  job_ll_node* node = linked_list_head(jobs);
  while (node != NULL) {
    job_ll_node* next = linked_list_next(node);
    if (node->job == job) {
      linked_list_remove(jobs, node);
      free(node);  // Free the node after removing it
      break;  // Exit after removing the job
    }
    node = next;
  }
}

/**
 * @brief Gets the job struct from the head of the jobs list.
 *
 * Useful for accessing the most recently added job (e.g., the current
 * foreground job if add_foreground_job was just called).
 *
 * @return A pointer to the job at the head of the list, or NULL if the list is empty.
 */
job* get_jobs_head() {
  job_ll_node* head = linked_list_head(jobs);
  return head ? head->job : NULL;  // Return NULL if no jobs in list
}

// TODO: add a cleanup function here so we free before exiting
// it can just call linked_list_clear
