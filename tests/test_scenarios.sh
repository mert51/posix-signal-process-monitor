#!/bin/bash
# Runs the main watchdog scenarios and prints PASS/FAIL for each one.
# Run it from the project root: bash tests/test_scenarios.sh

make clean && make || { echo "Build failed"; exit 1; }

FAIL=0
check() {   # check <description> <pattern in watchdog.log>
    if grep -q "$2" watchdog.log; then
        echo "PASS: $1"
    else
        echo "FAIL: $1"
        FAIL=1
    fi
}

./watchdog &
WD_PID=$!
sleep 2

echo "--- Test 1: SIGUSR1 status report ---"
kill -SIGUSR1 $WD_PID
sleep 1

echo "--- Test 2: Unexpected termination and restart limit ---"
# pgrep -n picks the newest loop worker under watchdog, so the same worker is
# killed each time: restarts 1/3, 2/3, 3/3, then the fourth kill gives GAVE_UP.
for i in 1 2 3 4; do
    W_PID=$(pgrep -n -P $WD_PID -f "workers/worker loop")
    if [ -n "$W_PID" ]; then
        kill -9 "$W_PID"
        sleep 1
    fi
done

echo "--- Test 3: Normal termination (worker-3 exits after 20 s) ---"
sleep 15

echo "--- Test 4: Graceful shutdown (SIGTERM) and zombie check ---"
kill -SIGTERM $WD_PID
sleep 3

echo "--- Log output ---"
cat watchdog.log
echo "--- Results ---"

check "Workers started"                 "worker-1 started"
check "SIGUSR1 status report written"   "status report"
check "Crash detected"                  "terminated unexpectedly"
check "Restart attempted"               "restarting .* (attempt 1/3)"
check "Restart limit reached (GAVE_UP)" "GAVE_UP"
check "Normal exit not restarted"       "exited normally"
check "Graceful shutdown completed"     "watchdog shutdown complete"

if kill -0 $WD_PID 2>/dev/null; then
    echo "FAIL: watchdog is still running"; FAIL=1
else
    echo "PASS: watchdog exited"
fi

if pgrep -P $WD_PID -f "workers/worker" > /dev/null; then
    echo "FAIL: worker processes left behind"; FAIL=1
else
    echo "PASS: no leftover workers"
fi

if ps -eo stat,args | grep "workers/worker" | grep -q '^Z'; then
    echo "FAIL: zombie process found"; FAIL=1
else
    echo "PASS: no zombie processes"
fi

exit $FAIL
