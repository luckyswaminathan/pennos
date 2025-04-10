#ifndef PENNSHELL_EXITING_SIGNAL_H_
#define PENNSHELL_EXITING_SIGNAL_H_

/*
 * @brief Set a signal handler for a given signal.
 * This function is a wrapper around sigaction.
 * This function will perror and exit if the signal handler cannot be set. This function
 * will set the SA_RESTART flag, but no other flags. It will also set sa_mask to
 * be empty.
 * @param sig the signal to set the handler for
 * @param handler the handler to set
 */
void exiting_set_signal_handler(int sig, void (*handler)(int));

#endif // PENNSHELL_EXITING_SIGNAL_H_
