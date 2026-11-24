#include <signal.h>
#include <stdio.h>
#include <time.h>
#include "logger.h"
#include "signal_manager.h"

/* Provided by the process manager (stubbed for now). */
int  pm_init(void);
int  pm_start_all(void);
void pm_handle_children(void);
void pm_periodic_check(void);
void pm_report_status(void);
void pm_stop_restarting(void);
void pm_terminate_all(void);
void pm_kill_remaining(void);
int  pm_alive_count(void);


#define SHUTDOWN_TIMEOUT_SEC 5

/*
 * Stop restarting workers, ask them to exit with SIGTERM, and give them
 * a few seconds. Anything still alive after that gets SIGKILL. Every
 * child is reaped so no zombies are left behind.
 */
static void graceful_shutdown(void)
{
    time_t start;
    struct timespec pause_for = {0, 100000000L}; /* 100 ms */

    log_write("INFO", 0, "shutdown requested, stopping workers");
    pm_stop_restarting();
    pm_terminate_all();

    start = time(NULL);
    while (pm_alive_count() > 0) {
        pm_handle_children();
        if (pm_alive_count() == 0)
            break;
        if (time(NULL) - start >= SHUTDOWN_TIMEOUT_SEC) {
            log_write("WARN", 0, "timeout reached, sending SIGKILL");
            pm_kill_remaining();
            break;
        }
        nanosleep(&pause_for, NULL);
    }

    pm_handle_children();   /* final reap */
    log_write("INFO", 0, "watchdog shutdown complete");
}

int main(void)
{
    sigset_t old_mask;

    if (log_open("watchdog.log") < 0)
        return 1;
    log_write("INFO", 0, "watchdog starting");

    if (signal_manager_init() < 0) {
        log_write("ERROR", 0, "could not install signal handlers");
        log_close();
        return 1;
    }

    /*
     * Block our signals so none can slip in between checking the flags
     * and going to sleep. sigsuspend() below unblocks them atomically.
     */
    if (signal_manager_block(&old_mask) < 0) {
        log_write("ERROR", 0, "could not block signals");
        log_close();
        return 1;
    }

    if (pm_init() < 0 || pm_start_all() < 0) {
        log_write("ERROR", 0, "could not start worker processes");
        log_close();
        return 1;
    }

        for (;;) {
        if (g_report_requested) {
            g_report_requested = 0;
            pm_report_status();
        }

        /* The process manager checks for dead children and the timer. */
        pm_handle_children();
        pm_periodic_check();

        /* Check this after the work above so a late report is not lost. */
        if (g_shutdown_requested)
            break;

        /* Sleep until any signal arrives, then loop and look at the flags. */
        sigsuspend(&old_mask);
    }
   

    graceful_shutdown();
    log_close();
    return 0;
}
