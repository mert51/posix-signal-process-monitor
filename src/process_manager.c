#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "process_manager.h"
#include "logger.h"
#include "signal_manager.h"

#define MAX_RESTARTS       3
#define CHECK_INTERVAL_SEC 5
#define MAX_WORKER_ARG 4

typedef enum {
    PM_IDLE = 0,
    PM_RUNNING,
    PM_EXITED_NORMAL,
    PM_GAVE_UP,
    PM_STOPPED
} pm_state_t;

typedef struct {
    const char *name;
    char       *argv[MAX_WORKER_ARG];
    pid_t       pid;
    pm_state_t  state;
    int         restart_count;
} worker_t;

static worker_t workers[] = {
    { "worker-1", { "./workers/worker", "loop",   "0",  NULL }, 0, PM_IDLE, 0 },
    { "worker-2", { "./workers/worker", "loop",   "0",  NULL }, 0, PM_IDLE, 0 },
    { "worker-3", { "./workers/worker", "normal", "20", NULL }, 0, PM_IDLE, 0 },
};
#define NUM_WORKERS ((int)(sizeof(workers) / sizeof(workers[0])))

static int shutting_down = 0;

static const char *state_name(pm_state_t s)
{
    switch (s) {
    case PM_IDLE:          return "IDLE";
    case PM_RUNNING:       return "RUNNING";
    case PM_EXITED_NORMAL: return "EXITED_NORMAL";
    case PM_GAVE_UP:       return "GAVE_UP";
    case PM_STOPPED:       return "STOPPED";
    }
    return "UNKNOWN";
}

static void describe_status(int status, char *out, size_t n)
{
    if (WIFEXITED(status))
        snprintf(out, n, "exit code %d", WEXITSTATUS(status));
    else if (WIFSIGNALED(status))
        snprintf(out, n, "killed by signal %d (%s)",
                 WTERMSIG(status), strsignal(WTERMSIG(status)));
    else
        snprintf(out, n, "unknown status 0x%x", (unsigned)status);
}

static int find_by_pid(pid_t pid)
{
    int i;
    for (i = 0; i < NUM_WORKERS; i++) {
        if (workers[i].pid == pid && workers[i].state == PM_RUNNING)
            return i;
    }
    return -1;
}

static int spawn_worker(int i)
{
    char msg[160];
    pid_t pid = fork();

    if (pid < 0) {
        snprintf(msg, sizeof(msg), "fork failed for %s: %s",
                 workers[i].name, strerror(errno));
        log_write("ERROR", 0, msg);
        return -1;
    }

    if (pid == 0) {
        sigset_t none;
        sigemptyset(&none);
        sigprocmask(SIG_SETMASK, &none, NULL);
        execv(workers[i].argv[0], workers[i].argv);
        _exit(127);
    }

    workers[i].pid   = pid;
    workers[i].state = PM_RUNNING;
    snprintf(msg, sizeof(msg), "%s started (%s %s %s)", workers[i].name,
             workers[i].argv[0], workers[i].argv[1], workers[i].argv[2]);
    log_write("START", pid, msg);
    return 0;
}

static void restart_or_give_up(int i)
{
    char msg[160];
    pid_t old_pid = workers[i].pid;

    if (workers[i].restart_count >= MAX_RESTARTS) {
        workers[i].state = PM_GAVE_UP;
        snprintf(msg, sizeof(msg), "%s reached restart limit (%d), giving up",
                 workers[i].name, MAX_RESTARTS);
        log_write("GAVE_UP", old_pid, msg);
        return;
    }

    workers[i].restart_count++;
    snprintf(msg, sizeof(msg), "restarting %s (attempt %d/%d)",
             workers[i].name, workers[i].restart_count, MAX_RESTARTS);
    log_write("RESTART", old_pid, msg);

    if (spawn_worker(i) < 0) {
        workers[i].state = PM_GAVE_UP;
        log_write("GAVE_UP", old_pid, "restart failed, giving up");
    }
}

static void record_exit(int i, int status)
{
    char how[96];
    char msg[256];
    pid_t pid = workers[i].pid;

    describe_status(status, how, sizeof(how));

    if (shutting_down) {
        workers[i].state = PM_STOPPED;
        snprintf(msg, sizeof(msg), "%s stopped during shutdown: %s",
                 workers[i].name, how);
        log_write("EXIT", pid, msg);
        return;
    }

    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
        workers[i].state = PM_EXITED_NORMAL;
        snprintf(msg, sizeof(msg), "%s exited normally (%s), not restarting",
                 workers[i].name, how);
        log_write("EXIT", pid, msg);
        return;
    }

    snprintf(msg, sizeof(msg), "%s terminated unexpectedly: %s",
             workers[i].name, how);
    log_write("CRASH", pid, msg);
    restart_or_give_up(i);
}

int pm_init(void)
{
    struct itimerval tv;
    int i;

    shutting_down = 0;
    for (i = 0; i < NUM_WORKERS; i++) {
        workers[i].pid           = 0;
        workers[i].state         = PM_IDLE;
        workers[i].restart_count = 0;
    }

    tv.it_value.tv_sec     = CHECK_INTERVAL_SEC;
    tv.it_value.tv_usec    = 0;
    tv.it_interval.tv_sec  = CHECK_INTERVAL_SEC;
    tv.it_interval.tv_usec = 0;
    if (setitimer(ITIMER_REAL, &tv, NULL) < 0) {
        log_write("ERROR", 0, "setitimer failed");
        return -1;
    }
    return 0;
}

int pm_start_all(void)
{
    int i;
    for (i = 0; i < NUM_WORKERS; i++) {
        if (spawn_worker(i) < 0)
            return -1;
    }
    return 0;
}

void pm_handle_children(void)
{
    int   status;
    pid_t pid;
    int   i;
    char  msg[96];

    g_sigchld_received = 0;

    for (;;) {
        pid = waitpid(-1, &status, WNOHANG);
        if (pid > 0) {
            i = find_by_pid(pid);
            if (i < 0) {
                snprintf(msg, sizeof(msg), "reaped unknown child");
                log_write("WARN", pid, msg);
            } else {
                record_exit(i, status);
            }
        } else if (pid == 0) {
            break;
        } else if (errno == EINTR) {
            continue;
        } else if (errno == ECHILD) { // Added for unexpected waitpid errors. Should be tested again with make clean make ./watchdog.
                break;
            } else {
                snprintf(msg, sizeof(msg), "waitpid unexpected error: %s", strerror(errno));
                log_write("WARN", 0, msg);
                break;
            }
    }
}

void pm_periodic_check(void)
{
    int  i;
    int  alive = 0;
    char msg[160];
    char alive_list[128] = "";

    if (!g_alarm_fired) return;
    g_alarm_fired = 0;

    if (shutting_down) return;

    for (i = 0; i < NUM_WORKERS; i++) {
        if (workers[i].state != PM_RUNNING)
            continue;
        if (kill(workers[i].pid, 0) < 0 && errno == ESRCH) {
            snprintf(msg, sizeof(msg), "%s no longer exists", workers[i].name);
            log_write("CRASH", workers[i].pid, msg);
            restart_or_give_up(i);
        }
        if (workers[i].state == PM_RUNNING) {
            alive++;
            char temp[64];
            snprintf(temp, sizeof(temp), "%s(%d) ", workers[i].name, workers[i].pid);
            strncat(alive_list, temp, sizeof(alive_list) - strlen(alive_list) - 1);
        }
        }

    snprintf(msg, sizeof(msg), "periodic check: %d/%d workers running [%s]",
             alive, NUM_WORKERS, alive_list);
    log_write("CHECK", 0, msg);
}

void pm_report_status(void)
{
    int  i;
    char msg[160];

    snprintf(msg, sizeof(msg), "status report (%d workers)", NUM_WORKERS);
    log_write("STATUS", getpid(), msg);

    for (i = 0; i < NUM_WORKERS; i++) {
        snprintf(msg, sizeof(msg), "%s state=%s restarts=%d/%d",
                 workers[i].name, state_name(workers[i].state),
                 workers[i].restart_count, MAX_RESTARTS);
        log_write("STATUS", workers[i].pid, msg);
    }
}

void pm_stop_restarting(void)
{
    struct itimerval off;

    shutting_down = 1;
    memset(&off, 0, sizeof(off));
    setitimer(ITIMER_REAL, &off, NULL);
}

void pm_terminate_all(void)
{
    int  i;
    char msg[96];

    shutting_down = 1;
    for (i = 0; i < NUM_WORKERS; i++) {
        if (workers[i].state != PM_RUNNING)
            continue;
        if (kill(workers[i].pid, SIGTERM) < 0 && errno != ESRCH)
            log_write("ERROR", workers[i].pid, "kill(SIGTERM) failed");
        snprintf(msg, sizeof(msg), "SIGTERM sent to %s", workers[i].name);
        log_write("SIGNAL", workers[i].pid, msg);
    }
}

void pm_kill_remaining(void)
{
    int   i;
    int   status;
    pid_t r;
    char  msg[96];

    shutting_down = 1;
    for (i = 0; i < NUM_WORKERS; i++) {
        if (workers[i].state != PM_RUNNING)
            continue;

        kill(workers[i].pid, SIGKILL);
        snprintf(msg, sizeof(msg), "SIGKILL sent to %s", workers[i].name);
        log_write("SIGNAL", workers[i].pid, msg);

        do {
            r = waitpid(workers[i].pid, &status, 0);
        } while (r < 0 && errno == EINTR);

        if (r == workers[i].pid)
            record_exit(i, status);
        else
            workers[i].state = PM_STOPPED;
    }
}

int pm_alive_count(void)
{
    int i;
    int n = 0;
    for (i = 0; i < NUM_WORKERS; i++) {
        if (workers[i].state == PM_RUNNING)
            n++;
    }
    return n;
}