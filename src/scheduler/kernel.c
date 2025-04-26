#include "src/scheduler/kernel.h"
#include "scheduler.h"
#include "logger.h"
#include "../../lib/exiting_alloc.h"
#include "../../lib/linked_list.h"
#include "spthread.h"
#include <string.h>
#include "shell/commands.h"
#include <stdlib.h> // For malloc/free
#include "src/utils/error_codes.h"


/**
 * @brief Duplicates the argument vector (argv).
 *
 * This is necessary to ensure the child process has its own independent copy
 * of the arguments. The original argv might be allocated on the parent's stack
 * or heap, and could be modified or freed by the parent after the child is
 * created, leading to use-after-free errors or data corruption for the child.
 * Duplication guarantees lifecycle independence and data integrity for the child's
 * arguments.
 *
 * @param argv The original null-terminated argument vector.
 * @return char** A pointer to the newly allocated and duplicated argument vector,
 *         or NULL if allocation fails. The caller is responsible for freeing
 *         this memory (including the individual strings) when the process terminates.
 */
static char** duplicate_argv(char *const argv[]) {
    if (!argv) {
        return NULL;
    }
    int argc = 0;
    while (argv[argc] != NULL) {
        argc++;
    }

    // Allocate space for the array of pointers (+1 for NULL terminator)
    char** new_argv = (char**) exiting_malloc(sizeof(char*) * (argc + 1));
    if (!new_argv) {
        return NULL;
    }

    // Duplicate each string
    for (int i = 0; i < argc; i++) {
        new_argv[i] = strdup(argv[i]);
        if (!new_argv[i]) {
            // Cleanup already duplicated strings
            for (int j = 0; j < i; j++) {
                free(new_argv[j]);
            }
            free(new_argv);
            return NULL;
        }
    }
    new_argv[argc] = NULL; // Null-terminate the array

    return new_argv;
}

/**
 * @brief Creates a new process control block (PCB) as a child of the given parent,
 *        initializes its thread, and adds it to the scheduler's ready queue.
 *
 * This function handles the complete setup of a new process:
 * 1. Allocates and initializes the PCB.
 * 2. Assigns a unique PID.
 * 3. Sets up parent-child relationship.
 * 4. Allocates and initializes the children list.
 * 5. Sets file descriptors, function pointer.
 * 6. Duplicates the command and arguments (`argv`).
 * 7. Allocates the spthread structure.
 * 8. Creates the underlying spthread using `spthread_create`.
 * 9. Sets the initial state and priority.
 * 10. Adds the process to the parent's children list.
 * 11. Adds the process to the appropriate scheduler ready queue using `k_add_to_ready_queue`.
 *
 * @param parent Pointer to the parent process's PCB. If NULL, the process is assumed
 *               to be an initial process (like init) with PPID 0.
 * @param func The function the new process should execute.
 * @param argv Null-terminated argument vector for the new process. The kernel copies this.
 * @return pid_t The PID of the newly created process, or -1 if any allocation or thread creation fails.
 */
pid_t k_proc_create(pcb_t *parent, void *(*func)(void *), char *const argv[]) {

    if (parent == NULL && scheduler_state->init_process != NULL) {
        return E_INIT_ALREADY_EXISTS;
    }

    pcb_t* proc = (pcb_t*) exiting_malloc(sizeof(pcb_t));
    if (!proc) {
        return E_FAILED_TO_ALLOCATE;
    }

    // Increment process count
    scheduler_state->process_count++;

    // Initialize core fields
    proc->pid = scheduler_state->process_count;
    proc->ppid = parent ? parent->pid : 0;
    proc->pgid = proc->pid; // New process starts a new process group
    proc->state = PROCESS_RUNNING; // Initial state
    proc->priority = parent == NULL ? PRIORITY_HIGH : PRIORITY_MEDIUM; // Default priority
    proc->sleep_time = 0.0;
    proc->exit_status = 0;
    proc->prev = NULL;
    proc->next = NULL;
    proc->waited_child = -2;
    for (int i = 0; i < PROCESS_FD_TABLE_SIZE; i++) {
        proc->process_fd_table[i].in_use = false;
    }

    proc->process_fd_table[STDIN_FD].in_use = true;
    proc->process_fd_table[STDIN_FD].mode = F_WRITE;
    proc->process_fd_table[STDIN_FD].global_fd = STDIN_FD;
    proc->process_fd_table[STDIN_FD].offset = 0;
    proc->process_fd_table[STDOUT_FD].in_use = true;
    proc->process_fd_table[STDOUT_FD].mode = F_READ;
    proc->process_fd_table[STDOUT_FD].global_fd = STDOUT_FD;
    proc->process_fd_table[STDOUT_FD].offset = 0;
    proc->process_fd_table[STDERR_FD].in_use = true;
    proc->process_fd_table[STDERR_FD].mode = F_READ;
    proc->process_fd_table[STDERR_FD].global_fd = STDERR_FD;
    proc->process_fd_table[STDERR_FD].offset = 0;

    // this is the init process
    if (parent == NULL) {
        scheduler_state->init_process = proc;
    }

    // Initialize children list
    proc->children = (child_process_ll_t) exiting_malloc(sizeof(*(proc->children)));
    if (!proc->children) {
        free(proc);
        return E_FAILED_TO_ALLOCATE;
    }
    // Initialize the allocated struct's members directly
    proc->children->head = NULL;
    proc->children->tail = NULL;
    proc->children->ele_dtor = NULL; // TODO: add a destructor for child_process_t

    // Set execution context and I/O
    proc->func = func;

    // Duplicate argv and set command
    proc->argv = duplicate_argv(argv);
    if (!proc->argv) {
        // duplicate_argv prints perror
        free(proc->children);
        free(proc);
        // This should be the only error that causes duplicate_argv to fail
        return E_FAILED_TO_ALLOCATE;
    }
    // Command is typically argv[0], ensure it's duplicated safely
    proc->command = (proc->argv && proc->argv[0]) ? strdup(proc->argv[0]) : NULL;
    if (proc->argv && proc->argv[0] && !proc->command) {
         // Cleanup argv array and strings
         for (int i = 0; proc->argv[i] != NULL; i++) {
             free(proc->argv[i]);
         }
         free(proc->argv);
         free(proc->children);
         free(proc);
         return E_BAD_ARGV;
    }

    // Allocate and create the thread
    proc->thread = (spthread_t*) exiting_malloc(sizeof(spthread_t));
    if (!proc->thread) {
        // Cleanup command and argv
        if (proc->command) free(proc->command);
        if (proc->argv) {
            for (int i = 0; proc->argv[i] != NULL; i++) {
                free(proc->argv[i]);
            }
            free(proc->argv);
        }
        free(proc->children);
        free(proc);
        return E_FAILED_TO_ALLOCATE;
    }

    // Pass the *duplicated* argv to spthread_create
    if (spthread_create(proc->thread, NULL, proc->func, proc->argv) != 0) {
        // Cleanup thread struct, command, argv
        free(proc->thread);
        if (proc->command) free(proc->command);
        if (proc->argv) {
            for (int i = 0; proc->argv[i] != NULL; i++) {
                free(proc->argv[i]);
            }
            free(proc->argv);
        }
        free(proc->children);
        free(proc);
        return E_FAILED_TO_ALLOCATE;
    }

    k_log("CREATED THREAD %lu for %s\n", proc->thread->thread, proc->command);

    child_process_t* child_process = (child_process_t*) exiting_malloc(sizeof(child_process_t));
    if (!child_process) {
        if (proc->command) free(proc->command);
        if (proc->argv) {
            for (int i = 0; proc->argv[i] != NULL; i++) {
                free(proc->argv[i]);
            }
            free(proc->argv);
        }
        free(proc->children);
        free(proc);
        return E_FAILED_TO_ALLOCATE;
    }
    child_process->process = proc;

    // Add to parent's children list
    if (parent && parent->children) {
        linked_list_push_tail(parent->children, child_process);
    }

    if (parent != NULL) {
        // copy the process file descriptor table to the child
        memcpy(proc->process_fd_table, parent->process_fd_table, sizeof(proc->process_fd_table));
    }


    // Add to scheduler ready queue (assuming k_add_to_ready_queue exists and is declared)
    // This function should likely reside in scheduler.c but be declared in kernel.h or scheduler.h (included by kernel.c)
    k_add_to_ready_queue(proc); // We'll need to ensure this function exists and is callable

    return proc->pid; // Return the new PID on success
}


/**
 * @brief Cleans up resources associated with a terminated or finished process.
 *
 * This function performs the necessary cleanup steps when a process is destroyed:
 * 1. Reparents any orphaned children of the process to the init process (PID 0).
 *    It assumes `scheduler_state` and `scheduler_state->init_process` are accessible.
 * 2. Removes the process from its parent's list of children.
 * 3. Frees allocated resources within the PCB, including the thread structure (if any),
 *    command string, argv array, and the children list structure.
 * 4. Frees the PCB structure itself.
 *
 * Note: This function does *not* handle removing the process from scheduler queues
 * (ready, blocked, etc.) or joining the underlying thread; those actions should
 * occur before calling cleanup.
 *
 * @param proc Pointer to the PCB of the process to clean up.
 */
int k_proc_cleanup(pcb_t *proc) {
    if (!proc) {
        return 0; // No error if proc is NULL
    }

    // 1. Reparent children to init process
    // Assumes scheduler_state and scheduler_state->init_process are valid and accessible
    if (scheduler_state && scheduler_state->init_process && proc->children) {
        child_process_t* child;
        // Pop children one by one, reparent, and add to init's list
        while ((child = linked_list_pop_head(proc->children)) != NULL) { 
            // Check if init process exists and has a children list before reparenting
            if (scheduler_state->init_process && scheduler_state->init_process->children) {
                child->process->ppid = scheduler_state->init_process->pid;
                linked_list_push_tail(scheduler_state->init_process->children, child);
            } else {
                // Orphaned child and no init process to adopt it? 
                // This is an error condition or requires a different orphan handling strategy.
                // For now, just log and free the child (leaking resources if it had children itself?)
                // We should ideally have a robust cleanup for such orphans too.
                // Maybe call k_proc_cleanup recursively? Risk of stack overflow?
                // Simplest for now might be to free its basic resources.
                if (child->process->command) free(child->process->command);
                if (child->process->argv) { 
                    for(int i=0; child->process->argv[i]; ++i) free(child->process->argv[i]);
                    free(child->process->argv);
                }
                if (child->process->thread) free(child->process->thread); // Thread struct, not joining here.
                if (child->process->children) free(child->process->children); // List struct itself
                free(child->process); // Free the child PCB
                free(child); // Free the child PCB
                return E_NO_INIT_PROCESS;
            }
        }
        // Free the (now empty) children list structure of the cleaned-up process
        free(proc->children);
        proc->children = NULL;
    }

    // 3. Free process resources (thread struct, command, argv)
    // Note: Joining the thread happens in k_reap_child.
    if (proc->thread) {
        // Note: Joining the thread should happen before cleanup, e.g., in waitpid.
        // Here we just free the spthread_t structure.
        free(proc->thread);
        proc->thread = NULL;
    }
    if (proc->command) {
        free(proc->command);
        proc->command = NULL;
    }
    if (proc->argv) {
        for (int i = 0; proc->argv[i] != NULL; i++) {
            free(proc->argv[i]);
        }
        free(proc->argv);
        proc->argv = NULL;
    }

    // 4. Free the PCB structure itself
    free(proc);
    return 0;
}
