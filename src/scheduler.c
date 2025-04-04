#include <stdio.h>
#include <stdlib.h> 
#include "scheduler.h"
#include "logger.h"
#include "../shell/exiting_alloc.h"
#include "../lib/linked_list.h"
#include "spthread.h"
#include <sys/time.h>
#include "kernel.h"


scheduler_t* scheduler_state = NULL;
static const int centisecond = 10000;
static int quantum = 0;
static sigset_t suspend_set;

static void alarm_handler(int signum) {}

void init_scheduler() {
    scheduler_state = (scheduler_t*) exiting_malloc(sizeof(scheduler_t));
    
    init_logger("scheduler.log");
    scheduler_state->process_count = 0;
    
    // Initialize all linked lists
    scheduler_state->processes.head = NULL;
    scheduler_state->processes.tail = NULL;
    scheduler_state->processes.ele_dtor = NULL;
    
    scheduler_state->priority_high.head = NULL;
    scheduler_state->priority_high.tail = NULL;
    scheduler_state->priority_high.ele_dtor = NULL;
    
    scheduler_state->priority_medium.head = NULL;
    scheduler_state->priority_medium.tail = NULL;
    scheduler_state->priority_medium.ele_dtor = NULL;
    
    scheduler_state->priority_low.head = NULL;
    scheduler_state->priority_low.tail = NULL;
    scheduler_state->priority_low.ele_dtor = NULL;
    
    scheduler_state->blocked_processes.head = NULL;
    scheduler_state->blocked_processes.tail = NULL;
    scheduler_state->blocked_processes.ele_dtor = NULL;
    
    scheduler_state->terminated_processes.head = NULL;
    scheduler_state->terminated_processes.tail = NULL;
    scheduler_state->terminated_processes.ele_dtor = NULL;

    scheduler_state->sleeping_processes.head = NULL;
    scheduler_state->sleeping_processes.tail = NULL;
    scheduler_state->sleeping_processes.ele_dtor = NULL;
    
    pcb_t* init = exiting_malloc(sizeof(pcb_t));
    init->ppid = 0;
    init->pid = scheduler_state->process_count++;
    init->pgid = init->pid;
    init->fd0 = -1;
    init->fd1 = -1;
    init->is_leader = true;
    init->sleep_time = 0;
    init->priority = PRIORITY_MEDIUM;
    init->state = PROCESS_RUNNING;
    init->prev = NULL;
    init->next = NULL;
    
    // Initialize thread first, then assign it
    spthread_t* thread = (spthread_t*)exiting_malloc(sizeof(spthread_t));
    if (spthread_create(thread, NULL, NULL, NULL) != 0) {
        LOG_ERROR("Failed to create init thread");
        exit(1);
    }
    init->thread = *thread;
    
    // Add init process to the medium priority queue
    linked_list_push_tail(&scheduler_state->priority_medium, init);
    
    scheduler_state->curr = init;
    scheduler_state->init = init;
    sigfillset(&suspend_set);
    sigdelset(&suspend_set, SIGALRM);

    struct sigaction act = (struct sigaction){
        .sa_handler = alarm_handler,
        .sa_mask = suspend_set,
        .sa_flags = SA_RESTART,
    };
    sigaction(SIGALRM, &act, NULL);
    sigset_t alarm_set;
    sigemptyset(&alarm_set);
    sigaddset(&alarm_set, SIGALRM);
    pthread_sigmask(SIG_UNBLOCK, &alarm_set, NULL);

    struct itimerval it;
    it.it_interval = (struct timeval){.tv_usec = centisecond * 10};
    it.it_value = it.it_interval; 
    setitimer(ITIMER_REAL, &it, NULL);
    // run_scheduler();

}

void decrease_sleep() {
    pcb_t* proc = scheduler_state->sleeping_processes.head;
    while (proc != NULL) {
        proc->sleep_time -= centisecond;
        if (proc->sleep_time <= 0) {
            proc->state = PROCESS_READY;
            linked_list_remove(&scheduler_state->sleeping_processes, proc);
            linked_list_push_tail(&scheduler_state->processes, proc);
        }
        proc = proc->next;
    }
}


void run_scheduler() {
    sigset_t mask;
    sigfillset(&mask);
    sigdelset(&mask, SIGALRM);
    
    while (1) {
        sigsuspend(&mask); 
        run_next_process();
        decrease_sleep();
        
    }
}

void run_next_process() {
    int current = quantum % 18;
    // Try high priority first if in high priority time slot
    if (current < 9) {
        if (scheduler_state->priority_high.head != NULL) {
            scheduler_state->curr = scheduler_state->priority_high.head;
            spthread_continue(scheduler_state->priority_high.head->thread);
            sigsuspend(&suspend_set);
            spthread_suspend(scheduler_state->priority_high.head->thread);
            pcb_t* proc = linked_list_pop_head(&scheduler_state->priority_high);
            linked_list_push_tail(&scheduler_state->priority_high, proc);
            quantum++;
            return;
        }
    }
    if (current < 15 || (current < 9 && scheduler_state->priority_high.head == NULL)) {
        if (scheduler_state->priority_medium.head != NULL) {
            scheduler_state->curr = scheduler_state->priority_medium.head;
            spthread_continue(scheduler_state->priority_medium.head->thread);
            sigsuspend(&suspend_set);
            spthread_suspend(scheduler_state->priority_medium.head->thread);
            pcb_t* proc = linked_list_pop_head(&scheduler_state->priority_medium);
            linked_list_push_tail(&scheduler_state->priority_medium, proc);
            quantum++;
            return;
        }
    }
    if (current >= 15 || 
        ((current < 15 && scheduler_state->priority_medium.head == NULL) && 
         (current < 9 && scheduler_state->priority_high.head == NULL))) {
        if (scheduler_state->priority_low.head != NULL) {
            scheduler_state->curr = scheduler_state->priority_low.head;
            spthread_continue(scheduler_state->priority_low.head->thread);
            sigsuspend(&suspend_set);
            spthread_suspend(scheduler_state->priority_low.head->thread);
            pcb_t* proc = linked_list_pop_head(&scheduler_state->priority_low);
            linked_list_push_tail(&scheduler_state->priority_low, proc);
            quantum++;
            return;
        }
        if (scheduler_state->priority_high.head != NULL) {
            scheduler_state->curr = scheduler_state->priority_high.head;
            spthread_continue(scheduler_state->priority_high.head->thread);
            sigsuspend(&suspend_set);
            spthread_suspend(scheduler_state->priority_high.head->thread);
            pcb_t* proc = linked_list_pop_head(&scheduler_state->priority_high);
            linked_list_push_tail(&scheduler_state->priority_high, proc);
        }
    }
    quantum++;
}

