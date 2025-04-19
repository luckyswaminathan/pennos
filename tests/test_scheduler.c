/* test_scheduler.c
 * Unit tests for scheduler queue operations.
 */

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include "src/scheduler/scheduler.h"
#include "lib/linked_list.h"

// Function declarations for scheduler functions
void add_process_to_queue(pcb_t* p);
void block_process(pcb_t* p);
void unblock_process(pcb_t* p);
void kill_process(pcb_t* p);
void continue_process(pcb_t* p);
void update_priority(pcb_t* p, priority_t new_priority);
void put_process_to_sleep(pcb_t* p, unsigned int ticks);
void cleanup_zombie_children(pcb_t* parent);

// Helper to create a PCB with given pid and priority
static pcb_t* create_pcb(int pid, priority_t priority) {
    pcb_t* p = malloc(sizeof(*p));
    assert(p != NULL);
    p->pid = pid; p->ppid = 0; p->pgid = 0;
    p->fd0 = 0; p->fd1 = 0;
    p->state = PROCESS_RUNNING;
    p->priority = priority;
    p->sleep_time = 0;
    p->children = malloc(sizeof(*p->children));
    assert(p->children != NULL);
    p->children->head = p->children->tail = NULL;
    p->children->ele_dtor = NULL;
    p->thread = NULL;
    p->func = NULL;
    p->command = NULL;
    p->argv = NULL;
    p->prev = p->next = NULL;
    return p;
}

// Reset scheduler_state to a fresh scheduler
static void reset_scheduler() {
    static scheduler_t local = {0};
    scheduler_state = &local;
    for (int i = 0; i < 3; i++) {
        scheduler_state->ready_queues[i].head = scheduler_state->ready_queues[i].tail = NULL;
        scheduler_state->ready_queues[i].ele_dtor = NULL;
    }
    scheduler_state->blocked_queue.head = scheduler_state->blocked_queue.tail = NULL;
    scheduler_state->blocked_queue.ele_dtor = NULL;
    scheduler_state->zombie_queue.head = scheduler_state->zombie_queue.tail = NULL;
    scheduler_state->zombie_queue.ele_dtor = NULL;
    scheduler_state->stopped_queue.head = scheduler_state->stopped_queue.tail = NULL;
    scheduler_state->stopped_queue.ele_dtor = NULL;
    scheduler_state->init_process = NULL;
    scheduler_state->current_process = NULL;
    scheduler_state->ticks = 0;
}

// Test functions
static void test_add_process_to_queue() {
    reset_scheduler();
    pcb_t* p = create_pcb(1, PRIORITY_LOW);
    add_process_to_queue(p);
    assert(scheduler_state->ready_queues[PRIORITY_LOW].head == p);
    assert(scheduler_state->ready_queues[PRIORITY_LOW].tail == p);
    free(p->children);
    free(p);
}

static void test_block_and_unblock() {
    reset_scheduler();
    pcb_t* p = create_pcb(2, PRIORITY_MEDIUM);
    add_process_to_queue(p);
    block_process(p);
    assert(scheduler_state->ready_queues[PRIORITY_MEDIUM].head == NULL);
    assert(scheduler_state->blocked_queue.head == p);
    unblock_process(p);
    assert(scheduler_state->blocked_queue.head == NULL);
    assert(scheduler_state->ready_queues[PRIORITY_MEDIUM].head == p);
    free(p->children);
    free(p);
}

static void test_kill_process() {
    reset_scheduler();
    pcb_t* p = create_pcb(3, PRIORITY_HIGH);
    add_process_to_queue(p);
    kill_process(p);
    assert(scheduler_state->ready_queues[PRIORITY_HIGH].head == NULL);
    assert(scheduler_state->zombie_queue.head == p);
    free(p->children);
    free(p);
}

static void test_continue_process() {
    reset_scheduler();
    pcb_t* p = create_pcb(4, PRIORITY_HIGH);
    linked_list_push_tail(&scheduler_state->blocked_queue, p);
    continue_process(p);
    assert(scheduler_state->blocked_queue.head == NULL);
    assert(scheduler_state->ready_queues[PRIORITY_HIGH].head == p);
    free(p->children);
    free(p);
}

static void test_update_priority() {
    reset_scheduler();
    pcb_t* p = create_pcb(5, PRIORITY_LOW);
    add_process_to_queue(p);
    update_priority(p, PRIORITY_HIGH);
    assert(scheduler_state->ready_queues[PRIORITY_LOW].head == NULL);
    assert(scheduler_state->ready_queues[PRIORITY_HIGH].head == p);
    free(p->children);
    free(p);
}

static void test_put_process_to_sleep() {
    reset_scheduler();
    pcb_t* p = create_pcb(6, PRIORITY_MEDIUM);
    add_process_to_queue(p);
    put_process_to_sleep(p, 7);
    assert(scheduler_state->ready_queues[PRIORITY_MEDIUM].head == NULL);
    assert(scheduler_state->blocked_queue.head == p);
    assert(p->sleep_time == 7);
    free(p->children);
    free(p);
}

static void test_cleanup_zombie_children() {
    reset_scheduler();
    pcb_t* parent = create_pcb(7, PRIORITY_LOW);
    parent->children->ele_dtor = NULL;
    pcb_t* c1 = create_pcb(8, PRIORITY_LOW);
    c1->state = PROCESS_ZOMBIED;
    pcb_t* c2 = create_pcb(9, PRIORITY_LOW);
    linked_list_push_tail(parent->children, c1);
    linked_list_push_tail(parent->children, c2);
    cleanup_zombie_children(parent);
    assert(parent->children->head == c2);
    assert(parent->children->tail == c2);
    free(c2->children);
    free(c2);
    free(parent->children);
    free(parent);
}

int main() {
    printf("Running scheduler unit tests...\n");
    test_add_process_to_queue();
    test_block_and_unblock();
    test_kill_process();
    test_continue_process();
    test_update_priority();
    test_put_process_to_sleep();
    test_cleanup_zombie_children();
    printf("All scheduler unit tests passed!\n");
    return 0;
}
