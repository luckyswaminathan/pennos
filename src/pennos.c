#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>

int main(void) {
    bool running = true;
    
    printf("PennOS starting...\n");
    
    while (running) {
        printf("pennos> ");
        fflush(stdout);
        sleep(1);  // Just to prevent busy-waiting
    }
    
    return 0;
}

