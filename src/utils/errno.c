#include "src/utils/errno.h"
#include "src/scheduler/sys.h"
#include <string.h>
char err_message[101]; // buffer for error message so we don't have to allocate memory

char* u_strerror(int errno) {
    switch (errno) {
        case E_NO_INIT_PROCESS:
            strcpy(err_message, "No init process"); break;
        case E_INIT_ALREADY_EXISTS:
            strcpy(err_message, "Init already exists"); break;
        case E_FAILED_TO_ALLOCATE:
            strcpy(err_message, "Failed to allocate"); break;
        case E_BAD_ARGV:
            strcpy(err_message, "Bad argv"); break;
        case E_STR_FORMAT_FAILED:
            strcpy(err_message, "Str format failed"); break;
        case E_STR_TOO_LONG_FOR_FPRINTF_BUF:
            strcpy(err_message, "Str too long for fprintf buf"); break;
        case E_INVALID_PCB:
            strcpy(err_message, "Invalid pcb"); break;
        case E_INVALID_SCHEDULER_STATE:
            strcpy(err_message, "Invalid scheduler state"); break;
        case E_NO_SUCH_PROCESS:
            strcpy(err_message, "No such process"); break;
        case E_INVALID_ARGUMENT:
            strcpy(err_message, "Invalid argument"); break;
        case E_TRIED_TO_KILL_INIT:
            strcpy(err_message, "Tried to kill init"); break;
        case E_TCSET_NO_TERMINAL_CONTROL:
            strcpy(err_message, "No terminal control"); break;
        case E_CONTINUE_NON_STOPPED_PROCESS:
            strcpy(err_message, "Continue non-stopped process"); break;
        case E_STOP_STOPPED_PROCESS:
            strcpy(err_message, "Stop stopped process"); break;
        case E_STOP_NON_ACTIVE_QUEUE_PROCESS:
            strcpy(err_message, "Stop non-active queue process"); break;
        case E_PID_NOT_FOUND:
            strcpy(err_message, "PID not found"); break;
        case E_NO_CURRENT_PROCESS:
            strcpy(err_message, "No current process"); break;
        case E_RUNNING_PROCESS_NOT_IN_READY_QUEUE:
            strcpy(err_message, "Running process not in ready queue"); break;

        // fs kernel errors
        case EK_OPEN_INVALID_FILENAME:
            strcpy(err_message, "Invalid filename"); break;
        case EK_OPEN_FIND_FILE_IN_ROOT_DIR_FAILED:
            strcpy(err_message, "Find file in root dir failed"); break;
        case EK_OPEN_GLOBAL_FD_TABLE_FULL:
            strcpy(err_message, "Global fd table full"); break;
        case EK_OPEN_FILE_DOES_NOT_EXIST:
            strcpy(err_message, "File does not exist"); break;
        case EK_OPEN_MALLOC_FAILED:
            strcpy(err_message, "Malloc failed"); break;
        case EK_OPEN_TIME_FAILED:
            strcpy(err_message, "Time failed"); break;
        case EK_OPEN_ALREADY_WRITE_LOCKED:
            strcpy(err_message, "Already write locked"); break;
        case EK_OPEN_WRITE_NEW_ROOT_DIR_ENTRY_FAILED:
            strcpy(err_message, "Write new root dir entry failed"); break;
        case EK_OPEN_NO_EMPTY_BLOCKS:
            strcpy(err_message, "No empty blocks"); break;
        case EK_OPEN_WRONG_PERMISSIONS:
            strcpy(err_message, "Wrong permissions"); break;
        case EK_OPEN_WRITE_ROOT_DIR_ENTRY_FAILED:
            strcpy(err_message, "Write root dir entry failed"); break;

        case EK_CLOSE_FD_OUT_OF_RANGE:
            strcpy(err_message, "FD out of range"); break;
        case EK_CLOSE_SPECIAL_FD:
            strcpy(err_message, "Special FD"); break;
        case EK_CLOSE_WRITE_ROOT_DIR_ENTRY_FAILED:
            strcpy(err_message, "Write root dir entry failed"); break;

        case EK_READ_FD_OUT_OF_RANGE:
            strcpy(err_message, "FD out of range"); break;
        case EK_READ_FD_NOT_IN_TABLE:
            strcpy(err_message, "FD not in table"); break;
        case EK_READ_COULD_NOT_JUMP_TO_BLOCK_FOR_OFFSET:
            strcpy(err_message, "Could not jump to block for offset"); break;
        case EK_READ_WRONG_PERMISSIONS:
            strcpy(err_message, "Wrong permissions"); break;
        case EK_READ_READ_FAILED:
            strcpy(err_message, "Read failed"); break;

        case EK_LSEEK_BAD_WHENCE:
            strcpy(err_message, "Lseek: Bad whence"); break;
        case EK_LSEEK_NEGATIVE_OFFSET:
            strcpy(err_message, "Lseek: Negative offset"); break;
        case EK_LSEEK_OFFSET_OVERFLOW:
            strcpy(err_message, "Offset overflow"); break;
        case EK_LSEEK_FD_OUT_OF_RANGE:
            strcpy(err_message, "FD out of range"); break;
        case EK_LSEEK_FD_NOT_IN_TABLE:
            strcpy(err_message, "FD not in table"); break;
        case EK_LSEEK_WRONG_PERMISSIONS:
            strcpy(err_message, "Lseek: Wrong permissions"); break;
        case EK_LSEEK_SPECIAL_FD:
            strcpy(err_message, "Special FD"); break;

        case EK_WRITE_WRONG_PERMISSIONS:
            strcpy(err_message, "Wrong permissions"); break;
        case EK_WRITE_FD_NOT_IN_TABLE:
            strcpy(err_message, "FD not in table"); break;
        case EK_WRITE_FD_OUT_OF_RANGE:
            strcpy(err_message, "FD out of range"); break;
        case EK_WRITE_GET_BLOCK_FAILED:
            strcpy(err_message, "Get block failed"); break;
        case EK_WRITE_NEXT_BLOCK_NUM_FAILED:
            strcpy(err_message, "Next block num failed"); break;
        case EK_WRITE_WRITE_BLOCK_FAILED:
            strcpy(err_message, "Write block failed"); break;
        case EK_WRITE_WRITE_ROOT_DIR_ENTRY_FAILED:
            strcpy(err_message, "Write root dir entry failed"); break;
        case EK_WRITE_NO_EMPTY_BLOCKS:
            strcpy(err_message, "No empty blocks"); break;
        case EK_WRITE_TIME_FAILED:
            strcpy(err_message, "Time failed"); break;
        case EK_WRITE_WRITE_FAILED:
            strcpy(err_message, "Write failed"); break;

        case EK_UNLINK_FILE_NOT_FOUND:
            strcpy(err_message, "Unlink: File not found"); break;
        case EK_UNLINK_FIND_FILE_IN_ROOT_DIR_FAILED:
            strcpy(err_message, "Unlink: Find file in root dir failed"); break;
        case EK_UNLINK_WRITE_ROOT_DIR_ENTRY_FAILED:
            strcpy(err_message, "Unlink: Write root dir entry failed"); break;
        case EK_UNLINK_INVALID_FILENAME:
            strcpy(err_message, "Unlink: Invalid filename"); break;

        case EK_LS_WRITE_FAILED:
            strcpy(err_message, "Write failed"); break;
        case EK_LS_FIND_FILE_IN_ROOT_DIR_FAILED:
            strcpy(err_message, "Find file in root dir failed"); break;
        case EK_LS_NOT_IMPLEMENTED:
            strcpy(err_message, "Not implemented"); break;
        case EK_LS_MALLOC_FAILED:
            strcpy(err_message, "Malloc failed"); break;
        case EK_LS_GET_BLOCK_FAILED:
            strcpy(err_message, "Get block failed"); break;
        case EK_LS_NEXT_BLOCK_NUM_FAILED:
            strcpy(err_message, "Next block num failed"); break;

        case EK_CHMOD_FILE_NOT_FOUND:
            strcpy(err_message, "File not found"); break;
        case EK_CHMOD_WRITE_ROOT_DIR_ENTRY_FAILED:
            strcpy(err_message, "Write root dir entry failed"); break;
        case EK_CHMOD_WRONG_PERMISSIONS:
            strcpy(err_message, "Wrong permissions"); break;
        case EK_CHMOD_INVALID_FILENAME:
            strcpy(err_message, "Invalid filename"); break;
        case EK_CHMOD_INVALID_MODE:
            strcpy(err_message, "Invalid mode"); break;

        case EK_MV_FILE_NOT_FOUND:
            strcpy(err_message, "File not found"); break;
        case EK_MV_WRONG_PERMISSIONS:
            strcpy(err_message, "Wrong permissions"); break;
        case EK_MV_UNLINK_FAILED:
            strcpy(err_message, "Unlink failed"); break;
        case EK_MV_INVALID_FILENAME:
            strcpy(err_message, "Invalid filename"); break;
        case EK_MV_OPEN_FAILED:
            strcpy(err_message, "Open failed"); break;
        case EK_MV_WRITE_ROOT_DIR_ENTRY_FAILED:
            strcpy(err_message, "Write root dir entry failed"); break;
        case EK_MV_CLOSE_FAILED:
            strcpy(err_message, "Close failed"); break;

        case EK_SETMODE_FD_OUT_OF_RANGE:
            strcpy(err_message, "FD out of range"); break;
        case EK_SETMODE_BAD_MODE:
            strcpy(err_message, "Bad mode"); break;
        case EK_SETMODE_FD_NOT_IN_USE:
            strcpy(err_message, "FD not in use"); break;

        case EK_GETMODE_FD_OUT_OF_RANGE:
            strcpy(err_message, "FD out of range"); break;
        case EK_GETMODE_FD_NOT_IN_USE:
            strcpy(err_message, "FD not in use"); break;

        // fs syscall errors
        case E_UNKNOWN_FD:
            strcpy(err_message, "Unknown FD"); break;
        case E_PROCESS_FILE_TABLE_FULL:
            strcpy(err_message, "Process file table full"); break;
        case E_READ_UNKNOWN_FD:
            strcpy(err_message, "Read: Unknown FD"); break;
        case E_STRING_FORMAT_FAILED:
            strcpy(err_message, "String format failed"); break;
        case E_STRING_TOO_LONG_FOR_PRINTF_BUF:
            strcpy(err_message, "String too long for printf buf"); break;

        default:
            strcpy(err_message, "Unknown error"); break;
    }
    return err_message;
}

void u_perror(const char *s) {
    int err_no = s_get_errno();
    if (err_no == 0) {
        return;
    }

    s_write(STDERR_FILENO, s, strlen(s));
    s_write(STDERR_FILENO, ": ", 2);
    // NOTE: there is a risk that if the above S_WRITE fails, the error message will be for the wrong error.
    // This is ok because we anticipate that this should never happen.
    char* error_message = u_strerror(err_no); // TODO: replace with actual error code once we store it in the PCB
    s_write(STDERR_FILENO, error_message, strlen(error_message));
    s_write(STDERR_FILENO, "\n", 1);
}
