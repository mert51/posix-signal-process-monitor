#include <signal.h>
#include <stdio.h>

#include "logger.h"
#include "signal_manager.h"

/* Provided by the process manager (stubbed for now). */
int  pm_init(void);
int  pm_start_all(void);
void pm_handle_children(void);
void pm_periodic_check(void);
void pm_report_status(void);

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
   

    log_write("INFO", 0, "shutdown requested");
    log_close();
    return 0;
}
