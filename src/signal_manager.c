#include <signal.h>
#include <string.h>

#include "signal_manager.h"

volatile sig_atomic_t g_shutdown_requested = 0;
volatile sig_atomic_t g_report_requested   = 0;
volatile sig_atomic_t g_sigchld_received   = 0;
volatile sig_atomic_t g_alarm_fired        = 0;

/*
 * The handlers do the bare minimum: set a flag and return. Anything
 * heavier (logging, waitpid, fork) is done later in the main loop.
 */
static void on_shutdown(int signum) { (void)signum; g_shutdown_requested = 1; }
static void on_usr1(int signum)     { (void)signum; g_report_requested   = 1; }
static void on_sigchld(int signum)  { (void)signum; g_sigchld_received   = 1; }
static void on_alarm(int signum)    { (void)signum; g_alarm_fired        = 1; }

/* The set of signals this module manages. */
static void fill_managed_set(sigset_t *set)
{
    sigemptyset(set);
    sigaddset(set, SIGINT);
    sigaddset(set, SIGTERM);
    sigaddset(set, SIGUSR1);
    sigaddset(set, SIGCHLD);
    sigaddset(set, SIGALRM);
}

static int install(int signum, void (*handler)(int), int flags)
{
    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handler;
    /* While one handler runs, hold back the other managed signals. */
    fill_managed_set(&sa.sa_mask);
    /* No SA_RESTART on purpose, so blocking calls return with EINTR. */
    sa.sa_flags = flags;
    return sigaction(signum, &sa, NULL);
}

int signal_manager_init(void)
{
    if (install(SIGINT,  on_shutdown, 0) < 0)            return -1;
    if (install(SIGTERM, on_shutdown, 0) < 0)            return -1;
    if (install(SIGUSR1, on_usr1,     0) < 0)            return -1;
    /* SA_NOCLDSTOP: only tell us when a child exits, not when it stops. */
    if (install(SIGCHLD, on_sigchld,  SA_NOCLDSTOP) < 0) return -1;
    if (install(SIGALRM, on_alarm,    0) < 0)            return -1;
    return 0;
}

int signal_manager_block(sigset_t *old)
{
    sigset_t set;

    fill_managed_set(&set);
    return sigprocmask(SIG_BLOCK, &set, old);
}

int signal_manager_restore(const sigset_t *old)
{
    return sigprocmask(SIG_SETMASK, old, NULL);
}
