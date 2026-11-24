#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

static volatile sig_atomic_t done = 0;

void term_handler(int sig) {
    (void)sig;
    done = 1;
}

int main(int argc, char **argv) {
    int sec = 0;
    struct sigaction sa;

    if (argc < 3) {
        fprintf(stderr, "usage: %s <normal|crash|loop> <seconds>\n", argv[0]);
        return 1;
    }

    if (sscanf(argv[2], "%d", &sec) != 1 || sec < 0) {
        fprintf(stderr, "invalid seconds: %s\n", argv[2]);
        return 1;
    }

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = term_handler;
    sigaction(SIGTERM, &sa, NULL);

    if (strcmp(argv[1], "normal") == 0) {
        sleep(sec);
        return 0;
    } else if (strcmp(argv[1], "crash") == 0) {
        sleep(sec);
        return 1;
    } else if (strcmp(argv[1], "loop") == 0) {
        while (!done) {
            pause();
        }
        return 0;
    }

    fprintf(stderr, "unknown mode: %s\n", argv[1]);
    return 1;
}
