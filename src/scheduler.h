

/* process control block design
So I need to create a process control block for each process.
Registers - type register_t
Program counter - type uintptr_t
Program status - type uintptr_t
Stack pointer - type uintptr_t
Process state - type enum process_state
Priority - type int
Scheduling parameters - type struct sched_param
Process ID - type pid_t
Parent process - type pid_t
Process group - type pid_t
Signals - type sigset_t
Time when process started - type time_t
CPU time used - type struct timeval
Children’s CPU time - type struct timeval
Time of next alarm - type struct timeval

*/