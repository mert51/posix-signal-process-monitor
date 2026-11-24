# Watchdog Process Monitoring System

Build: make
Run: ./watchdog
Status Report: kill -SIGUSR1 <watchdog_pid>
Graceful Shutdown: kill -SIGTERM <watchdog_pid>
Automated Test: ./tests/run_tests.sh
