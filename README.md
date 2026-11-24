# Watchdog Process Monitoring System

A POSIX signal-based process supervision system implemented in C for Linux environments. The `watchdog` supervisor spawns child worker processes, monitors their termination states, restarts failed workers up to a defined limit, generates runtime status reports, and performs graceful shutdowns without leaving zombie processes.

## Project Structure

```text
watchdog-project/
├── Makefile                  # Incremental build and cleanup rules
├── README.md                 # Installation, usage, and testing guide
├── include/                  # Module header files
│   ├── logger.h              # POSIX file logging interface
│   ├── process_manager.h     # Child process lifecycle structures
│   └── signal_manager.h      # Signal flags and handler setup
├── src/                      # C source implementations
│   ├── main.c                # Main supervision event loop
│   ├── signal_manager.c      # sigaction() registration
│   ├── process_manager.c     # fork(), execv(), waitpid(), restart logic
│   └── logger.c              # Unbuffered open/write/close logging
├── workers/                  # Simulated child worker executables
│   └── worker.c              # Supports normal, crash, and loop modes
├── tests/                    # Automated test scripts
│   └── test_scenarios.sh     # End-to-end verification suite
└── docs/                     # Project documentation
    └── REPORT.pdf            # Final project report
```

## Build Instructions

Compile both the `watchdog` supervisor and the `workers/worker` test executable using `make`:

```bash
make
```

To remove compiled object files, executables, and generated logs:

```bash
make clean
```

## Usage

Start the watchdog process in the foreground or background:

```bash
./watchdog
```

All lifecycle events (process creation, normal exit, crash, restart, status report, and shutdown) are recorded with timestamps in `watchdog.log`.

## Signal Controls and Manual Testing

Find the running `watchdog` PID (e.g., via `ps x | grep watchdog`) and send POSIX signals from a second terminal:

- **Runtime Status Report (`SIGUSR1`):** Writes the PID and current state of each child process to `watchdog.log`:
  ```bash
  kill -SIGUSR1 <WATCHDOG_PID>
  ```
- **Unexpected Worker Crash (`SIGKILL`):** Terminate a worker PID to trigger automatic restart (bounded to a maximum of 3 restarts):
  ```bash
  kill -SIGKILL <WORKER_PID>
  ```
- **Graceful Shutdown (`SIGTERM` or `Ctrl+C`):** Stops spawning new workers, terminates all active children, reaps them cleanly, and closes the log file:
  ```bash
  kill -SIGTERM <WATCHDOG_PID>
  ```

## Automated Test Suite

Run the automated test script to verify build, status reporting, restart limits, and zombie-free shutdown:

```bash
./tests/test_scenarios.sh
```
