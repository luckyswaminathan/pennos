#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include "scheduler.h"
#include "spthread.h"
#include "logger.h"

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
    
    // Spawn child process
    pid_t child_pid = s_spawn(child_func, NULL, -1, -1);
    LOG_INFO("Spawned child process with pid %d", child_pid);
    
    // Wait for child to finish
    int status;
    pid_t waited_pid = s_waitpid(child_pid, &status, false);
    LOG_INFO("Child process %d finished with status %d", waited_pid, status);
    
    return 0;
}
