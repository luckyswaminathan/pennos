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

    scheduler_state->stopped_processes.head = NULL;
    scheduler_state->stopped_processes.tail = NULL;
    scheduler_state->stopped_processes.ele_dtor = NULL;
    
    pcb_t* init = exiting_malloc(sizeof(pcb_t));
    init->ppid = 0;
    init->pid = scheduler_state->process_count++;
    init->pgid = init->pid;
    init->fd0 = -1;
    init->fd1 = -1;
    init->is_leader = true;
    init->priority = PRIORITY_MEDIUM;
    init->state = PROCESS_RUNNING;
    spthread_t* thread = (spthread_t*)exiting_malloc(sizeof(spthread_t));
    init->thread = *thread;
    spthread_create(thread, NULL, NULL, NULL);

    scheduler_state->curr = init;
    
    scheduler_state->init = init;


    sigset_t suspend_set;
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
    run_scheduler();

}



void run_scheduler() {
    sigset_t mask;
    sigfillset(&mask);
    sigdelset(&mask, SIGALRM);
    
    while (1) {
        sigsuspend(&mask); 
        handle_next_process();
    }
}

void handle_next_process() {
    int current = quantum % 18;
    if (current < 9) {

    } else if (current < 15) {

    } else {
        
    }
    quantum++;


    
    
}

