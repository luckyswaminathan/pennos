// File shared by kernel/shell about error codes


#ifndef PENNOS_ERROR_CODES_H
#define PENNOS_ERROR_CODES_H

#define E_NO_INIT_PROCESS -1
#define E_INIT_ALREADY_EXISTS -2
#define E_FAILED_TO_ALLOCATE -3
#define E_BAD_ARGV -4
#define E_STR_FORMAT_FAILED -5
#define E_STR_TOO_LONG_FOR_FPRINTF_BUF -6
#define E_INVALID_PCB -7
#define E_INVALID_SCHEDULER_STATE -8
#define E_NO_SUCH_PROCESS -9
#define E_INVALID_ARGUMENT -10
#define E_TRIED_TO_KILL_INIT -11
#endif // PENNOS_ERROR_CODES_H
