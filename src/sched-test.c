#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include "scheduler.h"
#include "spthread.h"
#include "logger.h"

// Thread function that counts forever
void* counter_thread(void* arg) {
    int id = *(int*)arg;
    int count = 0;
    while(1) {
        LOG_INFO("Thread %d counting: %d", id, count++);
        // Small delay to make output readable
        usleep(500000);  // 500ms delay
    }
    return NULL;
}

int main() {
    // Initialize logger to use file
    init_logger("scheduler.log");
    LOG_INFO("Starting scheduler test...");
    
    // Initialize scheduler
    init_scheduler();
    
    LOG_INFO("Creating threads...");
    spthread_t threads[3];
    int* thread_ids[3];
    
    for(int i = 0; i < 3; i++) {
        LOG_INFO("Creating thread %d", i + 1);
        thread_ids[i] = malloc(sizeof(int));
        *thread_ids[i] = i + 1;
        if (spthread_create(&threads[i], NULL, counter_thread, thread_ids[i]) != 0) {
            perror("Failed to create thread");
            exit(1);
        }
        // Create PCB and make thread ready
        pcb_t* pcb = create_process(threads[i], 0, false);
        if (!pcb) {
            perror("Failed to create PCB");
            exit(1);
        }
        make_process_ready(pcb);
        LOG_INFO("Created thread %d", i + 1);
        
    }
    
    LOG_INFO("All threads created, starting scheduler...");
    
    // Run the scheduler
    run_scheduler();
    
    return 0;  // Never reached
}
