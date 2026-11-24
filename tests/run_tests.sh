#!/bin/bash

make clean
make
if [ $? -ne 0 ]; then
    echo "Build failed"
    exit 1
fi

./watchdog &
PID=$!
sleep 2

kill -SIGUSR1 $PID
sleep 1

ps x | grep -E "watchdog|worker"

kill -SIGTERM $PID
sleep 3

ps x | grep -E "watchdog|worker"
cat watchdog.log
