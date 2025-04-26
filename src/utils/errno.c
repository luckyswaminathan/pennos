#include "src/utils/errno.h"
#include "src/scheduler/sys.h"
#include <string.h>

char* u_strerror(int errno) {
    // TODO: implement this
    return NULL;
}

void u_perror(const char *s) {
    s_write(STDERR_FILENO, s, strlen(s));
    s_write(STDERR_FILENO, ":\n", 2);
    // NOTE: there is a risk that if the above S_WRITE fails, the error message will be for the wrong error.
    // This is ok because we anticipate that this should never happen.
    char* error_message = u_strerror(-1); // TODO: replace with actual error code once we store it in the PCB
    if (error_message == NULL) {
        char* error_message = "FAILED TO ALLOCATE MEMORY FOR ERROR MESSAGE\n";
        s_write(STDERR_FILENO, error_message, strlen(error_message));
        return;
    }
    s_write(STDERR_FILENO, error_message, strlen(error_message));
    s_write(STDERR_FILENO, "\n", 1);
    free(error_message);
}
