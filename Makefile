CC = gcc
CFLAGS = -Wall -Wextra -g -Iinclude

EXE = watchdog
WORKER = workers/worker
OBJS = src/main.o src/signal_manager.o src/process_manager.o src/logger.o

all: $(EXE) $(WORKER)

$(EXE): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

src/main.o: src/main.c include/signal_manager.h include/process_manager.h include/logger.h
	$(CC) $(CFLAGS) -c $< -o $@

src/signal_manager.o: src/signal_manager.c include/signal_manager.h
	$(CC) $(CFLAGS) -c $< -o $@

src/process_manager.o: src/process_manager.c include/process_manager.h include/logger.h
	$(CC) $(CFLAGS) -c $< -o $@

src/logger.o: src/logger.c include/logger.h
	$(CC) $(CFLAGS) -c $< -o $@

$(WORKER): workers/worker.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(OBJS) $(EXE) $(WORKER) watchdog.log

.PHONY: all clean
