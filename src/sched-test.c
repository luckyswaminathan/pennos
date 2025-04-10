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
    
    // Run forever, incrementing a counter
    for (int i = 0; i < 3; i++) {
        LOG_INFO("Child process iteration %d", i);
        usleep(1000000); // Sleep for 1 second between iterations
    }

    LOG_INFO("Child process finished");
    return NULL;
}

void* child_func2(void* arg) {
    LOG_INFO("****Child process 2 started");
    
    // Run forever, incrementing a counter
    for (int i = 0;i<3; i++) {
        LOG_INFO("*****Child process iteration %d", i);
        usleep(1000000); // Sleep for 1 second between iterations
    }

    LOG_INFO("Child process 2 finished");
    return NULL;
}

static void* child_func3(void* arg) {
    LOG_INFO("****Child process 3 started");
    
    for (int i = 0;i<15; i++) {
        LOG_INFO("*****Child process 3 iteration %d", i);
        usleep(1000000); // Sleep for 1 second between iterations
    }

    LOG_INFO("Child process 3 finished");
    return NULL;
}

int main() {
    // Initialize logger to use file
    init_logger("scheduler.log");
    LOG_INFO("Starting scheduler test...");
    
    init_scheduler();
    LOG_INFO("Scheduler initialized");
    
    // Spawn child processes
    pid_t child_pid = s_spawn(child_func, NULL, -1, -1);
    pid_t child_pid2 = s_spawn(child_func2, NULL, -1, -1);
    pid_t child_pid3 = s_spawn(child_func3, NULL, -1, -1);
    LOG_INFO("Spawned child process with pid %d", child_pid);
    LOG_INFO("Spawned child process with pid %d", child_pid2);
    LOG_INFO("Spawned child process with pid %d", child_pid3);
    
    // Set process 3 to low priority
    s_nice(child_pid3, PRIORITY_HIGH);
    s_kill(child_pid3);
    log_all_processes();
    // Start the scheduler - this will run until all processes finish
    run_scheduler();
    
    
    LOG_INFO("All processes finished");
    return 0;
}
