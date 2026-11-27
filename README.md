# Watchdog Process Monitoring System

A POSIX signal-based process supervision system implemented in C for Linux environments. The `watchdog` supervisor spawns child worker processes, monitors their termination states via `SIGCHLD` and `waitpid()`, performs periodic health checks every 5 seconds (`SIGALRM`), restarts unexpectedly terminated workers up to 3 times, generates runtime status reports (`SIGUSR1`), and executes a controlled graceful shutdown (`SIGINT`/`SIGTERM`) without leaving zombie processes.

## Team Members

- **Student 1 (Signal Management & Main Control Loop):** Mert Önder (`mert51`)
- **Student 2 (Child Process Management):** Mert Ali Maden (`denl678`)
- **Student 3 (Logging & Test Infrastructure):** Civancan Aktaş (`CivancanAktas`)

## Project Structure

```text
posix-signal-process-monitor/
├── Makefile                  # Incremental build and cleanup rules
├── README.md                 # Installation, usage, and testing guide
├── docs/                     # Project documentation and verification logs
│   ├── REPORT.pdf            # Final consolidated group project report
│   ├── REPORT_S1.pdf         # Student 1 module report
│   └── s1_evidence/          # Manual signal and edge-case test logs
│       ├── ctrl_c_testi.txt
│       ├── double_sigint_testi.txt
│       ├── restart_testi.txt
│       └── sigkill_timeout_testi.txt
├── include/                  # Module header files
│   ├── logger.h              # POSIX file logging interface
│   ├── process_manager.h     # Child process lifecycle structures
│   └── signal_manager.h      # Signal flags and handler setup
├── src/                      # C source implementations
│   ├── main.c                # Main supervision loop (sigsuspend) and shutdown sequence
│   ├── signal_manager.c      # Async-signal-safe sigaction() registration
│   ├── process_manager.c     # fork(), execv(), waitpid(), restart & periodic check logic
│   └── logger.c              # Unbuffered open/write/close logging
├── workers/                  # Simulated child worker executables
│   └── worker.c              # Supports normal, crash, and loop modes
└── tests/                    # Automated test scripts
    └── test_scenarios.sh     # End-to-end verification suite with PASS/FAIL checks
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

Upon startup, `watchdog` spawns three managed child workers:
- `worker-1` (`./workers/worker loop 0`) — runs continuously until terminated
- `worker-2` (`./workers/worker loop 0`) — runs continuously until terminated
- `worker-3` (`./workers/worker normal 20`) — runs for 20 seconds and exits cleanly with status `0` (not restarted)

All lifecycle events (`INFO`, `START`, `STATUS`, `CHECK`, `CRASH`, `RESTART`, `GAVE_UP`, `WARN`, `SIGNAL`, `EXIT`) are recorded with timestamps in `watchdog.log`.

## Signal Controls and Manual Testing

Find the running `watchdog` PID (e.g., via `ps x | grep watchdog`) and send POSIX signals from a second terminal:

- **Runtime Status Report (`SIGUSR1`):** Writes the PID, current state (`RUNNING`, `EXITED_NORMAL`, `GAVE_UP`, `STOPPED`), and restart count of each worker to `watchdog.log`:
  ```bash
  kill -SIGUSR1 <WATCHDOG_PID>
  ```
- **Unexpected Worker Crash (`SIGKILL`):** Terminate a worker PID to trigger `SIGCHLD` detection and automatic restart (bounded to `MAX_RESTARTS = 3`, after which the worker transitions to `GAVE_UP`):
  ```bash
  kill -SIGKILL <WORKER_PID>
  ```
- **Graceful Shutdown (`SIGTERM` or `Ctrl+C` / `SIGINT`):** Stops restarting workers, disables the periodic timer, forwards `SIGTERM` to all active children, waits up to 5 seconds (escalating to `SIGKILL` if any child remains), reaps all processes, and closes `watchdog.log`:
  ```bash
  kill -SIGTERM <WATCHDOG_PID>
  ```

## Automated Test Suite and Evidence

Run the automated test script to verify compilation, `SIGUSR1` status reporting, `SIGKILL` crash recovery (`GAVE_UP` after 3 restarts), normal worker exit, and zombie-free `SIGTERM` shutdown with automated `PASS`/`FAIL` checks:

```bash
bash tests/test_scenarios.sh
```

Additional manual edge-case test logs (`Ctrl+C`, repeated `SIGINT`, restart limit, and `SIGKILL` shutdown timeout escalation) are documented under `docs/s1_evidence/`.
