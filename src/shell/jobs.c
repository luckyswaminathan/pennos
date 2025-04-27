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

static job_ll _jobs = {
  .head = NULL,
  .tail = NULL,
  .ele_dtor = NULL
};
static job_ll* jobs = &_jobs;

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
}

void handle_jobs() {
  job_ll_node* node = linked_list_head(jobs);

  while (node != NULL) {
    s_fprintf_short(STDERR_FILENO, "[%lu] ", node->job->id);
    print_job_command(node->job);
    s_fprintf_short(STDERR_FILENO, "\n");
    node = linked_list_next(node);
  }
}

void print_all_jobs() {
  job_ll_node* node = linked_list_head(jobs);
  while (node != NULL) {
    fprintf(stderr, "[%lu] ", node->job->id);
    print_job_command(node->job);
    fprintf(stderr, "\n");
    node = linked_list_next(node);
  }
}


void handle_fg(struct parsed_command* cmd) {
  LOG_INFO("FOREGROUND");
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

  // mark the job as running
  job->status = J_RUNNING_FG;

  // Give terminal control to the job
  tcsetpgrp(STDIN_FILENO, job->pids[0]);

  if (job->status == J_STOPPED) {
    kill(-job->pids[0], SIGCONT);
  }
  LOG_INFO("Waiting for foreground job %ld", job->id);
  s_waitpid(job->pids[0], NULL, true);

  // Give terminal control back to the shell
  tcsetpgrp(STDIN_FILENO, getpid());
}

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

  job->status = J_RUNNING_BG;

  // Resume the job in the background
  kill(-job->pids[0], SIGCONT);

  fprintf(stderr, "Running: ");
  print_job_command(job);
  fprintf(stderr, "\n");
}

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
  print_job_list();
}

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

void continue_job(job* job) {
  job->status = J_RUNNING_FG;
  execute_job(job);
}

job* find_job_by_pid(pid_t pid) {
  job_ll_node* node = linked_list_head(jobs);

  while (node) {
    if (node->job->pids != NULL) {
      fprintf(stderr, "Comparing pid %d with job pid %d\n", pid, node->job->pids[0]);
      if (node->job->pids[0] == pid) {
        return node->job;
      }
    }
    node = linked_list_next(node);
  }

  fprintf(stderr, "No job found with pid %d\n", pid);
  return NULL;
}

// TODO: what is this used for
void print_job_list() {
  job_ll_node* node = linked_list_head(jobs);
  while (node) {
    print_job(node->job);
    node = linked_list_next(node);
  }
}

void remove_job_by_pid(pid_t pid) {
  LOG_INFO("Removing job with PID %d", pid);
  job_ll_node* node = linked_list_head(jobs);

  while (node != NULL) {
    if (node->job->pids[0] == pid) {
      break; // we've found the node with the PID
    }
    node = linked_list_next(node);
  }

  if (node != NULL) {
    linked_list_remove(jobs, node);

    // Print completion message
    fprintf(stderr, "Finished ");
    print_parsed_command(node->job->cmd);

    // Free the job resources
    destroy_job(node->job);
    free(node);
  } else {
    // Job not found - this is expected for foreground jobs that were already cleaned up
    LOG_INFO("No job found for PID %d - may have been already removed", pid);
  }
}

void add_foreground_job(job* job) {
  job_ll_node* node = (job_ll_node*) exiting_malloc(sizeof(job_ll_node));
  node->prev = NULL;
  node->next = NULL;
  node->job = job;
  linked_list_push_head(jobs, node);
}

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

job* get_jobs_head() {
  job_ll_node* head = linked_list_head(jobs);
  return head ? head->job : NULL;  // Return NULL if no jobs in list
}

// TODO: add a cleanup function here so we free before exiting
// it can just call linked_list_clear
