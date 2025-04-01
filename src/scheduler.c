#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include "./scheduler.h"
#include "./spthread.h"
#include "./logger.h"

// Global scheduler state
static scheduler_state_t scheduler;
static const int TIME_SLICE_USEC = 10000; // 10ms time slice

// Forward declarations
static void schedule_next_process(void);


void alarm_handler(int signum) {
    LOG_DEBUG("Alarm signal received");
    schedule_next_process();
}

void run_scheduler(void) {
    LOG_INFO("Starting scheduler...");
    struct itimerval it;
    it.it_interval = (struct timeval){.tv_usec = TIME_SLICE_USEC};
    it.it_value = it.it_interval;
    setitimer(ITIMER_REAL, &it, NULL);
    
    // If no process is ready, schedule one
    if (!scheduler.current) {
        schedule_next_process();
    }
    
    // Main scheduler loop - just wait for signals
    LOG_INFO("Entering scheduler loop");
    while(true) {
        pause();
    }
}

// Initialize the scheduler
void init_scheduler(void) {
    LOG_INFO("Initializing scheduler...");
    
    // Initialize scheduler state
    scheduler.ready_head = NULL;
    scheduler.blocked_head = NULL;
    scheduler.current = NULL;
    scheduler.all_processes = NULL;
    scheduler.process_count = 0;
    scheduler.fg_process = NULL;

    // Set up timer for preemption
    struct sigaction sa = {
        .sa_handler = alarm_handler,
        .sa_flags = SA_RESTART,
    };
    sigemptyset(&sa.sa_mask);
    sigaction(SIGALRM, &sa, NULL);

    sigset_t alarm_set;
    sigemptyset(&alarm_set);
    sigaddset(&alarm_set, SIGALRM);
    pthread_sigmask(SIG_UNBLOCK, &alarm_set, NULL);

    LOG_INFO("Scheduler initialized, process_count=%d", scheduler.process_count);
}

// Create a new process
pcb_t* create_process(spthread_t thread, pid_t ppid, bool is_background) {
    LOG_INFO("Creating new process with ppid=%d", ppid);
    pcb_t* pcb = malloc(sizeof(pcb_t));
    if (!pcb) {
        LOG_ERROR("Failed to allocate PCB");
        return NULL;
    }

    // Initialize PCB fields
    pcb->pid = scheduler.process_count++;
    pcb->ppid = ppid;
    pcb->pgid = pcb->pid;  // By default, process is its own group leader
    pcb->is_leader = true;
    pcb->thread = thread;
    pcb->state = PROCESS_READY;
    pcb->is_background = is_background;
    
    // Initialize list pointers
    pcb->prev = pcb->next = NULL;
    pcb->first_child = NULL;
    pcb->next_sibling = NULL;

    LOG_INFO("Created process with pid=%d", pcb->pid);
    return pcb;
}

// Add process to ready queue
void make_process_ready(pcb_t* pcb) {
    if (!pcb) {
        LOG_WARN("Attempted to make NULL process ready");
        return;
    }
    LOG_INFO("Making process %d ready", pcb->pid);

    pcb->state = PROCESS_READY;
    pcb->prev = NULL;  // Clear old prev pointer
    pcb->next = scheduler.ready_head;
    if (scheduler.ready_head) {
        scheduler.ready_head->prev = pcb;
    }
    scheduler.ready_head = pcb;
    LOG_INFO("Process %d added to ready queue", pcb->pid);
}

// Block a process
void block_process(pcb_t* pcb) {
    if (!pcb) return;

    pcb->state = PROCESS_BLOCKED;
    pcb->next = scheduler.blocked_head;
    if (scheduler.blocked_head) {
        scheduler.blocked_head->prev = pcb;
    }
    scheduler.blocked_head = pcb;
}

// Core scheduling function
static void schedule_next_process(void) {
    LOG_DEBUG("Scheduling next process");
    LOG_DEBUG("Current process: %s", scheduler.current ? "yes" : "no");
    LOG_DEBUG("Ready queue head: %s", scheduler.ready_head ? "yes" : "no");
    if (scheduler.current) {
        // Save current process state
        pcb_t* old = scheduler.current;
        // Only suspend if it's still running
        if (old->state == PROCESS_RUNNING) {
            LOG_DEBUG("Moving current process %d back to ready queue", old->pid);
            old->state = PROCESS_READY;
            make_process_ready(old);
            spthread_suspend(old->thread);
        }
    }

    // Get next process from ready queue
    pcb_t* next = scheduler.ready_head;
    if (next) {
        // Remove from ready queue
        scheduler.ready_head = next->next;
        if (scheduler.ready_head) {
            scheduler.ready_head->prev = NULL;
        }
        next->next = next->prev = NULL;

        // Make it running
        next->state = PROCESS_RUNNING;
        scheduler.current = next;
        LOG_INFO("Switching to process %d", next->pid);
        spthread_continue(next->thread);
    } else {
        scheduler.current = NULL;
    }
}

// Add a child process to parent's children list
void add_child_process(pcb_t* parent, pcb_t* child) {
    if (!parent || !child) return;

    child->next_sibling = parent->first_child;
    parent->first_child = child;
    child->ppid = parent->pid;
}

// Clean up a terminated process
void cleanup_process(pcb_t* pcb) {
    if (!pcb) return;

    // Remove from any queues it might be in
    if (pcb->prev) pcb->prev->next = pcb->next;
    if (pcb->next) pcb->next->prev = pcb->prev;

    if (pcb == scheduler.ready_head) scheduler.ready_head = pcb->next;
    if (pcb == scheduler.blocked_head) scheduler.blocked_head = pcb->next;
    if (pcb == scheduler.current) scheduler.current = NULL;

    // Free resources
    free(pcb->command);
    free(pcb);
}
