#ifndef SIGNAL_MANAGER_H
#define SIGNAL_MANAGER_H

#include <signal.h>

/*
 * Signal handlers only set these flags. The real work happens in the
 * main loop, which keeps the handlers async-signal-safe.
 */
extern volatile sig_atomic_t g_shutdown_requested; /* set by SIGINT / SIGTERM */
extern volatile sig_atomic_t g_report_requested;   /* set by SIGUSR1 */
extern volatile sig_atomic_t g_sigchld_received;   /* set by SIGCHLD */
extern volatile sig_atomic_t g_alarm_fired;        /* set by SIGALRM */

/* Install all signal handlers with sigaction(). Returns 0, or -1 on error. */
int signal_manager_init(void);

/* Block the signals we manage and save the previous mask in *old. */
int signal_manager_block(sigset_t *old);

/* Put the saved mask back. Also used by a child right after fork(). */
int signal_manager_restore(const sigset_t *old);

#endif

