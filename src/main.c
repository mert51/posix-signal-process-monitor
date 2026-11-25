#include <signal.h>
#include <stdio.h>
#include <time.h>
#include "logger.h"
#include "signal_manager.h"

#include "process_manager.h"


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
        /* Some workers may already be running; shut them down cleanly. */
        graceful_shutdown();
        log_close();
        return 1;
    }

           for (;;) {
        if (g_report_requested) {
            g_report_requested = 0;
            pm_report_status();
        }

        /*
         * Check for shutdown before reaping. On Ctrl+C the workers get
         * SIGINT too and die; reaping them first would count that as a
         * crash and restart them.
         */
        if (g_shutdown_requested)
            break;

        /* The process manager checks for dead children and the timer. */
        pm_handle_children();
        pm_periodic_check();

        /* Sleep until any signal arrives, then loop and look at the flags. */
        sigsuspend(&old_mask);
    }

    graceful_shutdown();
    log_close();
    return 0;
}
x
