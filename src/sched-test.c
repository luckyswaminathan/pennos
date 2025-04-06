#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include "scheduler.h"
#include "spthread.h"
#include "logger.h"
#include "sys.h"

void* child_func(void* arg) {
    LOG_INFO("Child process started");
    sleep(10); // Simulate some work

    LOG_INFO("Child process finished");
    return NULL;
}

int main() {
    // Initialize logger to use file
    init_logger("scheduler.log");
    LOG_INFO("Starting scheduler test...");
    
    init_scheduler();
    LOG_INFO("Scheduler initialized");
    
    // Spawn child process
    pid_t child_pid = s_spawn(child_func, NULL, -1, -1);
    LOG_INFO("Spawned child process with pid %d", child_pid);
    log_all_processes();
    // Start the scheduler - this will run until all processes finish
    run_scheduler();
    
    LOG_INFO("All processes finished");
    return 0;
}
