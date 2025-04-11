#include "scheduler.h"



pcb_t* k_proc_create(pcb_t *parent, int fd0, int fd1);

void k_proc_cleanup(pcb_t *proc);
