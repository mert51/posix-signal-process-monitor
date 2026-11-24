#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "logger.h"

static int fd = -1;

int log_open(const char *filename) {
    fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        perror("open");
        return -1;
    }
    return 0;
}

void log_write(const char *type, pid_t pid, const char *msg) {
    char buf[256];
    char tstr[32];
    time_t t;
    struct tm *tm_ptr;
    int len;

    if (fd < 0) {
        return;
    }

    t = time(NULL);
    tm_ptr = localtime(&t);
    strftime(tstr, sizeof(tstr), "%Y-%m-%d %H:%M:%S", tm_ptr);

    len = snprintf(buf, sizeof(buf), "[%s] [%s] pid=%d: %s\n", tstr, type, (int)pid, msg);
    if (len > 0) {
        if (write(fd, buf, strlen(buf)) < 0) {
            perror("write");
        }
    }
}

void log_close(void) {
    if (fd >= 0) {
        if (close(fd) < 0) {
            perror("close");
        }
        fd = -1;
    }
}
