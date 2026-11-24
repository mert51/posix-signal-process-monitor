#!/bin/bash
make clean && make || { echo "Build failed"; exit 1; }

./watchdog &
WD_PID=$!
sleep 2

echo "--- Test 1: SIGUSR1 Status Report ---"
kill -SIGUSR1 $WD_PID
sleep 1

echo "--- Test 2: Unexpected Termination & Restart Limit (SIGKILL on loop worker) ---"
for i in 1 2 3 4; do
    W_PID=$(pgrep -f "workers/worker loop" | head -n 1)
    if [ -n "$W_PID" ]; then
        kill -9 $W_PID
        sleep 2
    fi
done

echo "--- Test 3: Graceful Shutdown (SIGTERM) & Zombie Check ---"
kill -SIGTERM $WD_PID
sleep 3
ps x | grep -E "watchdog|worker"

echo "--- Log Output ---"
cat watchdog.log
