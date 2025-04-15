#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <stdbool.h>
#include <features.h>
#include <sys/types.h>

// Flag to indicate if the process should continue running
static volatile bool running = true;

// Define a large number for the busy loop
#define BUSY_LOOP_ITERATIONS 100000000

/**
 * Signal handler for SIGINT (Ctrl+C)
 */
void sigint_handler(int sig) {
    printf("\nReceived SIGINT, stopping busy process...\n");
    running = false;
}

/**
 * Main function - consumes CPU cycles until interrupted
 */
int main(int argc, char *argv[]) {
    // Set up signal handler for SIGINT
    signal(SIGINT, sigint_handler);
    
    printf("Starting CPU-intensive process. Press Ctrl+C to stop.\n");
    
    // Main busy loop
    unsigned long long counter = 0;
    while (running) {
        // Perform a CPU intensive operation
        for (int i = 0; i < BUSY_LOOP_ITERATIONS && running; i++) {
            // Just burning CPU cycles
            counter++;
        }
        
        // Periodically output the counter to show we're still running
        printf("Processed %llu iterations\n", counter);
    }
    
    printf("Process terminated after %llu iterations\n", counter);
    return EXIT_SUCCESS;
} 