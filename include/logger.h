#ifndef LOGGER_H
#define LOGGER_H

#include <sys/types.h>

int log_open(const char *filename);
void log_write(const char *type, pid_t pid, const char *msg);
void log_close(void);

#endif
