#include "scheduler.h"
#include "kernel.h"
#include "logger.h"
#include <errno.h>
#include "../../lib/exiting_alloc.h"
#include "../../lib/linked_list.h"
#include "spthread.h"

/**
 * @brief Create a child process that executes the function `func`.
 * The child will retain some attributes of the parent.
 *
 * @param func Function to be executed by the child process.
 * @param argv Null-terminated array of args, including the command name as argv[0].
 * @param fd0 Input file descriptor.
 * @param fd1 Output file descriptor.
 * @return pid_t The process ID of the created child process.
 */
pid_t s_spawn(void *(*func)(void *), void *arg)
{
    log_queue_state();
    LOG_INFO("s_spawn called with parent %d", scheduler_state->curr->pid);
    pcb_t *proc = k_proc_create(scheduler_state->curr, arg);
    linked_list_push_tail(&scheduler_state->curr->children, proc, child_pointers.prev, child_pointers.next);
    pcb_t *child = scheduler_state->curr->children.head;
    LOG_INFO("child %d", child->pid);
    log_queue_state();
    proc->thread = (spthread_t *)exiting_malloc(sizeof(spthread_t));
    if (spthread_create(proc->thread, NULL, func, arg) != 0)
    {
        LOG_ERROR("Failed to create thread for process %d", proc->pid);
        return -1;
    }
    printf("CREATED THREAD %lu %s\n", proc->thread->tid, proc->command);
    return proc->pid;
}

/**
 * @brief Wait on a child of the calling process, until it changes state.
 * If `nohang` is true, this will not block the calling process and return immediately.
 *
 * @param pid Process ID of the child to wait for.
 * @param wstatus Pointer to an integer variable where the status will be stored.
 * @param nohang If true, return immediately if no child has exited.
 * @return pid_t The process ID of the child which has changed state on success, -1 on error.
 */
pid_t s_waitpid(pid_t pid, int *wstatus, bool nohang)
{
    LOG_INFO("s_waitpid called with pid %d, nohang %d", pid, nohang);
    log_process_state();
    LOG_INFO("s_waitpid called with pid %d, nohang %d", pid, nohang);
    LOG_INFO("curr %d", scheduler_state->curr->pid);
    pcb_t* proc = scheduler_state->curr->children.head;
    //pcb_t* curr = scheduler_state->curr;
    while (proc != NULL) {
        if (pid == -1 || proc->pid == pid) {
            LOG_INFO("Found running process %d", proc->pid);

            if (proc->state == PROCESS_ZOMBIED || proc->state == PROCESS_STOPPED)
            {
                // Log that we're waiting on this process
                log_waited(proc->pid, proc->priority, proc->command);
                linked_list_remove(&scheduler_state->processes, proc, process_pointers.prev, process_pointers.next);
                linked_list_push_tail(&scheduler_state->terminated_processes, proc, priority_pointers.prev, priority_pointers.next);
                return proc->pid;
            }

            if (nohang)
            {
                return 0;
            }
            else
            {
                // Block until process completes
                int status = spthread_join(*proc->thread, (void **)wstatus);
                if (status != 0)
                {
                    return -1;
                }
                proc->state = PROCESS_ZOMBIED;
                linked_list_remove(&scheduler_state->processes, proc, process_pointers.prev, process_pointers.next);
                linked_list_push_tail(&scheduler_state->terminated_processes, proc, priority_pointers.prev, priority_pointers.next);

                // Log that we've waited on this process
                log_waited(proc->pid, proc->priority, proc->command);

                return proc->pid;
            }
        }
        fprintf(stderr, "seg fault is here?\n");
        proc = proc->process_pointers.next;
    }
    return 0;
}

/**
 * @brief Send a signal to a particular process.
 *
 * @param pid Process ID of the target proces.
 * @param signal Signal number to be sent.
 * @return 0 on success, -1 on error.
 */
int s_kill(pid_t pid)
{
    fprintf(stderr, "killing process %d\n", pid);
    pcb_t *proc = scheduler_state->processes.head;
    while (proc != NULL)
    {
        if (proc->pid == pid)
        {
            LOG_INFO("Killing process %d", proc->pid);
            int priority = proc->priority;
            if (priority == PRIORITY_HIGH)
            {
                linked_list_remove(&scheduler_state->priority_high, proc, priority_pointers.prev, priority_pointers.next);
            }
            else if (priority == PRIORITY_MEDIUM)
            {
                linked_list_remove(&scheduler_state->priority_medium, proc, priority_pointers.prev, priority_pointers.next);
            }
            else if (priority == PRIORITY_LOW)
            {
                linked_list_remove(&scheduler_state->priority_low, proc, priority_pointers.prev, priority_pointers.next);
            }
            proc->state = PROCESS_STOPPED;
            linked_list_push_tail(&scheduler_state->terminated_processes, proc, priority_pointers.prev, priority_pointers.next);

            // Log the process being signaled
            log_signaled(proc->pid, proc->priority, proc->command);

            spthread_cancel(*proc->thread);
            return 0;
        }
        else if (proc->ppid == pid) {
            log_orphan(proc->pid, proc->priority, proc->command);
            // set the parent pid to be
            LOG_INFO("freeing child %d", proc->pid);
            proc->ppid = 0;
            linked_list_push_tail(&scheduler_state->init->children, proc, child_pointers.prev, child_pointers.next);
        }
        proc = proc->process_pointers.next;
    }
    return -1;
}

int s_nice(pid_t pid, int priority)
{
    pcb_t *proc = scheduler_state->processes.head;
    while (proc != NULL)
    {
        if (proc->pid == pid)
        {
            LOG_INFO("Setting priority of process %d to %d", proc->pid, priority);
            if (proc->priority != priority)
            {
                int old_priority = proc->priority;

                if (priority == PRIORITY_HIGH)
                {
                    if (proc->priority == PRIORITY_MEDIUM)
                    {
                        linked_list_remove(&scheduler_state->priority_medium, proc, priority_pointers.prev, priority_pointers.next);
                    }
                    else if (proc->priority == PRIORITY_LOW)
                    {
                        linked_list_remove(&scheduler_state->priority_low, proc, priority_pointers.prev, priority_pointers.next);
                    }
                    proc->priority = priority;
                    linked_list_push_tail(&scheduler_state->priority_high, proc, priority_pointers.prev, priority_pointers.next);
                }
                else if (priority == PRIORITY_MEDIUM)
                {
                    if (proc->priority == PRIORITY_HIGH)
                    {
                        linked_list_remove(&scheduler_state->priority_high, proc, priority_pointers.prev, priority_pointers.next);
                    }
                    else if (proc->priority == PRIORITY_LOW)
                    {
                        linked_list_remove(&scheduler_state->priority_low, proc, priority_pointers.prev, priority_pointers.next);
                    }
                    proc->priority = priority;
                    linked_list_push_tail(&scheduler_state->priority_medium, proc, priority_pointers.prev, priority_pointers.next);
                }
                else if (priority == PRIORITY_LOW)
                {
                    if (proc->priority == PRIORITY_HIGH)
                    {
                        linked_list_remove(&scheduler_state->priority_high, proc, priority_pointers.prev, priority_pointers.next);
                    }
                    else if (proc->priority == PRIORITY_MEDIUM)
                    {
                        linked_list_remove(&scheduler_state->priority_medium, proc, priority_pointers.prev, priority_pointers.next);
                    }
                    proc->priority = priority;
                    linked_list_push_tail(&scheduler_state->priority_low, proc, priority_pointers.prev, priority_pointers.next);
                }

                // Log the priority change
                log_nice(proc->pid, old_priority, priority, proc->command);
            }
            return 0;
        }
        proc = proc->process_pointers.next;
    }
    return -1;
}

/**
 * @brief Send a stop signal to a process (similar to SIGSTOP).
 *
 * @param pid Process ID of the target process.
 * @return 0 on success, -1 on error.
 */
int s_stop(pid_t pid)
{
    pcb_t *proc = scheduler_state->processes.head;
    while (proc != NULL)
    {
        if (proc->pid == pid)
        {
            LOG_INFO("Stopping process %d", proc->pid);

            // Call the stop_process function to handle the actual stopping
            stop_process(proc);

            // We don't actually suspend the thread here, just change its state
            // In a real OS, this would send a SIGSTOP signal

            return 0;
        }
        proc = proc->process_pointers.next;
    }
    return -1;
}

/**
 * @brief Send a continue signal to a process (similar to SIGCONT).
 *
 * @param pid Process ID of the target process.
 * @return 0 on success, -1 on error.
 */
int s_cont(pid_t pid)
{
    // First check in the standard process list
    pcb_t *proc = scheduler_state->processes.head;
    while (proc != NULL)
    {
        if (proc->pid == pid && proc->state == PROCESS_BLOCKED)
        {
            LOG_INFO("Continuing process %d", proc->pid);

            // Call the continue_process function to handle the actual continuation
            continue_process(proc);

            return 0;
        }
        proc = proc->process_pointers.next;
    }

    // Also check in the blocked_processes list
    proc = scheduler_state->blocked_processes.head;
    while (proc != NULL)
    {
        if (proc->pid == pid)
        {
            LOG_INFO("Continuing blocked process %d", proc->pid);

            // Call the continue_process function to handle the actual continuation
            continue_process(proc);

            return 0;
        }
        proc = proc->priority_pointers.next;
    }

    return -1;
}

/**
 * @brief Unconditionally exit the calling process.
 */
void s_exit(void)
{
    PANIC("s_exit not implemented");
}

// void s_sleep(unsigned int ticks) {
//     put_process_to_sleep(scheduler_state->curr, ticks);
//     run_scheduler();
// }
