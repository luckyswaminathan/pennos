#include "./Job.h"
#include "./parser.h"
#include <stdio.h>
#include <stdlib.h>

void destroy_job(job* job) {
    free(job->cmd);
    free(job);
}
