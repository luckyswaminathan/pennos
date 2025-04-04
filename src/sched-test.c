#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include "scheduler.h"
#include "spthread.h"
#include "logger.h"

int main() {
    // Initialize logger to use file
    init_logger("scheduler.log");
    LOG_INFO("Starting scheduler test...");
    
    init_scheduler();
    
    return 0;
}
