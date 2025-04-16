#include "scheduler.h"



pcb_t* k_proc_create(pcb_t *parent, void* arg);

void k_proc_reparent_children(pcb_t *proc);
