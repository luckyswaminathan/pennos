#include "./Job.h"
#include "./parser.h"
#include <stdio.h>
#include <stdlib.h>

void print_job(job* job) {
    // TODO: this prints to stdout, not stderr and also is not formatted correctly
    print_parsed_command(job->cmd);
    if (job->status == J_STOPPED) {
        fprintf(stderr, " (stopped)");
    } else if (job->status == J_RUNNING_BG) {
        fprintf(stderr, " (running)");
    }
}

void destroy_job(job* job) {
    free(job->cmd);
    free(job);
}
