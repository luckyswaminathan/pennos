#include "./spthread.h"
#include "./scheduler.h"


void init_scheduler(scheduler_t* scheduler) {
    scheduler->process_tree = NULL;
    scheduler->running = NULL;
    scheduler->ready_queue = linked_list(pcb_t);
    scheduler->blocked_queue = linked_list(pcb_t);
    scheduler->ele_dtor = NULL;
    scheduler->preempt_flag = false;
    scheduler->process_count = 0;
}
