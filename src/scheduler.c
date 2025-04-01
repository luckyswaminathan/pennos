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
    it.it_interval = (struct timeval){.tv_sec = 0, .tv_usec = TIME_SLICE_USEC};
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

    // Initialize priority scheduling counters
    scheduler.quantum_count = 0;
    for (int i = 0; i < NUM_PRIORITIES; i++) {
        scheduler.priority_quanta[i] = 0;
    }

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

    // Set initial priority - shell gets high priority
    pcb->priority = (pcb->pid == 0) ? PRIORITY_HIGH : PRIORITY_MEDIUM;
    
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

    // Update quantum counts if we have a running process
    if (scheduler.current) {
        scheduler.quantum_count++;
        scheduler.priority_quanta[scheduler.current->priority]++;
        LOG_DEBUG("Quantum counts - Total: %lu, High: %lu, Med: %lu, Low: %lu",
                 scheduler.quantum_count,
                 scheduler.priority_quanta[PRIORITY_HIGH],
                 scheduler.priority_quanta[PRIORITY_MEDIUM],
                 scheduler.priority_quanta[PRIORITY_LOW]);

        // Save current process state
        pcb_t* old = scheduler.current;
        // Only suspend if it's still running and not non-preemptible
        if (old->state == PROCESS_RUNNING && !old->non_preemptible) {
            LOG_DEBUG("Moving current process %d back to ready queue", old->pid);
            old->state = PROCESS_READY;
            make_process_ready(old);
            spthread_suspend(old->thread);
        } else if (old->non_preemptible) {
            LOG_DEBUG("Not preempting non-preemptible process %d", old->pid);
            return;
        }
    }

    // Calculate priority ratios if we have enough quanta
    int target_priority = -1;
    if (scheduler.quantum_count > 0) {
        float high_ratio = scheduler.priority_quanta[PRIORITY_HIGH] / (float)scheduler.quantum_count;
        float med_ratio = scheduler.priority_quanta[PRIORITY_MEDIUM] / (float)scheduler.quantum_count;
        float low_ratio = scheduler.priority_quanta[PRIORITY_LOW] / (float)scheduler.quantum_count;

        LOG_DEBUG("Priority ratios - High: %.2f, Med: %.2f, Low: %.2f",
                 high_ratio, med_ratio, low_ratio);

        // Determine which priority level should run next based on target ratios
        if (high_ratio < 0.6) {
            target_priority = PRIORITY_HIGH;
        } else if (med_ratio < 0.2) {
            target_priority = PRIORITY_MEDIUM;
        } else if (low_ratio < 0.2) {
            target_priority = PRIORITY_LOW;
        }
    }

    // Find next process to run
    pcb_t* next = scheduler.ready_head;
    pcb_t* best = NULL;

    // First try to find a process of the target priority
    if (target_priority != -1) {
        while (next) {
            if (next->priority == target_priority) {
                best = next;
                break;
            }
            next = next->next;
        }
    }

    // If no process of target priority found, take first ready process
    if (!best && scheduler.ready_head) {
        best = scheduler.ready_head;
    }

    // Switch to the chosen process
    if (best) {
        // Remove from ready queue
        if (best->prev) {
            best->prev->next = best->next;
        } else {
            scheduler.ready_head = best->next;
        }
        if (best->next) {
            best->next->prev = best->prev;
        }
        best->next = best->prev = NULL;

        // Make it running
        best->state = PROCESS_RUNNING;
        scheduler.current = best;
        LOG_INFO("Switching to process %d (priority %d)", best->pid, best->priority);
        spthread_continue(best->thread);
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
