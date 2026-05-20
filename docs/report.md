# AutoHeal — Self-Healing Process Manager & Anomaly Detector
### Operating Systems Final Project Report

**Course:** Operating Systems  
**Team:** Sumaica Rehman · Syed Ali Zaidi · Abdul Mateen · Abdul Ahad  
**Date:** May 2026  
**Repository:** https://github.com/ahad58041/Autoheal

---

## 1. Problem Statement

Modern operating systems provide no automated response to runaway processes. A CPU-spinning loop, a memory leak, or a hung process silently degrades system performance until a human operator intervenes manually. AutoHeal solves this by acting as a user-space process supervisor that continuously monitors the system, detects anomalies through a rules engine, and autonomously applies a graduated healing response — all visible in real time through a live web dashboard.

---

## 2. System Architecture

AutoHeal is a multi-threaded C++17 daemon comprising five modules wired together through shared, mutex-protected data structures.

```
/proc filesystem
      │
      ▼
  OBSERVER (Thread A)         samples every 1 second via /proc
      │ SnapshotBuffer
      ▼
  BRAIN   (Thread B)          applies threshold rules over a 5-sample window
      │ AnomalyEvent queue
      ▼
  HEALER  (Thread C)          SIGSTOP → SIGTERM → SIGKILL escalation ladder
      │ InterventionLog
      ▼
  INTERFACE (Thread D)        WebSocket server on port 8080, JSON broadcast
      │ ws://localhost:8080
      ▼
  Next.js Dashboard           live process table, anomaly banner, intervention log
```

**Key design invariants:**
- Observer never blocks on a single `/proc` read — a process can die mid-read; the exception is caught and the PID is skipped for that tick.
- `SnapshotBuffer` is single-producer / multi-consumer protected by `std::mutex`.
- Healer never signals PIDs in the ignore list (PID 1, kernel threads, the AutoHeal process itself, the parent shell).
- The Brain requires **sustained** anomalous behavior across N consecutive samples before flagging — eliminating one-shot false positives.
- All strings crossing the WebSocket boundary are JSON-escaped and length-capped at 128 characters.

---

## 3. Module Descriptions

### 3.1 Observer (`src/observer/`)
**Owner: Sumaica Rehman**

The Observer runs on a dedicated thread and samples every process on the system once per second. For each PID it opens `/proc/<pid>/stat` and `/proc/<pid>/status`, extracting:

- `comm` — process name
- `state` — R/S/D/Z/T
- `utime_ticks`, `stime_ticks` — raw CPU tick counters
- `rss_kb`, `vsize_kb` — memory usage
- `ppid`, `uid`

CPU percentage is computed between two consecutive snapshots:

```
cpu% = (Δticks / ticks_per_sec) / Δt_sec × 100
```

where `ticks_per_sec = sysconf(_SC_CLK_TCK)` and `Δt_sec` is the measured wall-clock interval between samples. This gives per-core CPU percentage (a single-threaded spinner reads ~100%, not 100%/N).

Each completed sweep is stored in `SnapshotBuffer`, which holds the latest snapshot and a rolling history of the previous 10. The Observer is the only writer; the Brain and Interface are readers.

### 3.2 Engine (`src/engine/`)
**Owner: Syed Ali Zaidi**

The Engine is the daemon's orchestrator. It:

1. Optionally daemonizes the process (`fork` + `setsid` + redirect stdio to `/dev/null`) unless `--foreground` is passed.
2. Registers `SIGTERM` / `SIGINT` handlers that set a global `shutdown_flag`.
3. Constructs all four module objects and launches each on its own `std::thread`.
4. Joins all threads cleanly on shutdown, ensuring no resources leak.

The Engine enforces the ignore list at startup by populating it with PID 1, all kthreads (comm starting with `[`), its own PID, and its parent's PID (the shell).

### 3.3 Brain (`src/brain/`)
**Owner: Abdul Mateen**

The Brain consumes the `SnapshotBuffer` and applies three detection rules:

| Rule | Trigger | Window |
|---|---|---|
| `CPU_SPIN` | CPU% > 90% for 5 consecutive samples | 5 seconds |
| `MEM_LEAK` | RSS grows > 10 MB/sec sustained | 5 seconds |
| `HUNG` | Process in D-state for 8 consecutive samples | 8 seconds |

Each process is tracked in a per-PID `Track` struct holding a circular window of recent samples. Only when a PID exceeds the threshold for the full window length does the Brain emit an `AnomalyEvent`. A 5-second cool-down prevents re-flagging the same PID immediately after an intervention.

Detection rules and all numeric thresholds are centralized in `src/common/config.hpp` — no magic numbers appear anywhere else in the codebase.

### 3.4 Healer (`src/healer/`)
**Owner: Abdul Mateen**

The Healer consumes `AnomalyEvent` objects from a thread-safe queue and applies a three-rung escalation ladder:

1. **SIGSTOP** — freezes the process immediately, halting CPU consumption while the operator is notified.
2. **Wait 3 seconds** — grace period.
3. **SIGTERM** — politely requests the process to clean up and exit.
4. **Wait 3 seconds** — grace period.
5. **SIGKILL** — unconditional termination if the process has not exited.

Before each signal the Healer re-checks the ignore list, verifying the PID is still the same process (not a recycled PID) and that the process still exists. Each action is recorded in the `InterventionLog` with a timestamp, PID, process name, signal sent, and outcome.

In `--dry-run` mode all signals are logged but not sent, enabling safe testing as a non-root user.

### 3.5 Interface (`src/interface/`)
**Owner: Abdul Ahad**

The Interface module runs a WebSocket server on port 8080 using the **websocketpp** library (header-only, backed by Boost.Asio). Every second it serializes the current `SnapshotBuffer` and `InterventionLog` into a JSON payload using **nlohmann/json** and broadcasts it to all connected clients:

```json
{
  "ts": 1748000000000,
  "host_pid": 613,
  "ws_port": 8080,
  "processes": [
    { "pid": 1, "comm": "systemd", "state": "S", "cpu": 0.0, "rss_kb": 11376, ... }
  ],
  "interventions": [
    { "at_ms": 1748000005000, "pid": 1004, "comm": "rogue_cpu",
      "reason": "CPU_SPIN", "stage": "SIGKILL", "signal": 9, "outcome": "sent" }
  ]
}
```

---

## 4. Dashboard (`dashboard/`)
**Owner: Abdul Ahad**

The dashboard is a Next.js 14 + TypeScript + Tailwind CSS single-page application. It connects to `ws://localhost:8080` on load and re-renders on every incoming JSON frame (1 Hz).

**Components:**
- `StatusHeader` — connection state dot (green/amber/red), host PID, process count.
- `AnomalyBanner` — red alert bar listing active anomalies (deduplicated by PID). Green "System healthy" banner when idle.
- `ProcessTable` — sortable table of all running processes; rows with active anomalies are highlighted in red; supports filter-by-name/PID.
- `InterventionFeed` — chronological log of every signal sent, showing PID, process name, reason, stage, and timestamp.

No server-side rendering is used — the page is a pure client-side WebSocket consumer, keeping the stack simple and the latency minimal.

---

## 5. Detection Logic

### CPU Spin Detection

Raw CPU ticks from `/proc/<pid>/stat` fields 14 (`utime`) and 15 (`stime`) are summed. Between two consecutive 1-second samples:

```
Δticks = (utime₂ + stime₂) − (utime₁ + stime₁)
cpu%   = (Δticks / CLK_TCK) / Δt × 100
```

This gives a number in the range `[0, 100]` per logical CPU core. A single-threaded busy loop reaches ~100%. The threshold is 90% sustained over 5 samples.

### Memory Leak Detection

RSS (resident set size) is read from `/proc/<pid>/status` field `VmRSS`. The Brain tracks the RSS value at the start of the window and at the current sample:

```
growth_rate = (rss_now − rss_window_start) / window_seconds  (KB/sec)
```

If `growth_rate > 10 240 KB/sec` (10 MB/sec) sustained for 5 seconds, `MEM_LEAK` is emitted.

### Hung Process Detection

A process in the `D` (uninterruptible sleep) state for 8 consecutive samples is flagged as `HUNG`. This typically indicates a stuck I/O wait. The Healer skips SIGSTOP for hung processes (they cannot respond) and escalates directly to SIGTERM → SIGKILL.

---

## 6. Safety & Security

| Concern | Mitigation |
|---|---|
| Signaling wrong PID (PID recycling) | Re-read `/proc/<pid>/comm` at signal time; skip if name changed |
| Killing critical system processes | Hardcoded ignore list: PID 1, all kthreads, AutoHeal itself, parent shell |
| False positives from burst activity | Sustained window (5 samples) required before any action |
| Non-root operation during development | `--dry-run` logs signals without sending; all dev testing run as non-root |
| WebSocket injection | All strings length-capped and JSON-escaped via nlohmann before broadcast |

---

## 7. Demo Rogue Programs (`rogues/`)

Three test programs were written to provide reproducible, controllable anomalies:

- **`rogue_cpu`** — a tight busy loop (`while(true) {}`) that pegs one CPU core at 100%.
- **`rogue_mem`** — allocates 50 MB per second in 1 MB chunks, capped at 1 GB to avoid OOM-killing the WSL VM.
- **`rogue_fork`** — forks children in a loop, capped at 50 total children, to test process-count monitoring.

---

## 8. Build & Run

### Prerequisites (Ubuntu 22.04 / WSL2)
```bash
sudo apt install -y build-essential libwebsocketpp-dev libboost-all-dev nlohmann-json3-dev nodejs
```

### Build
```bash
make clean && make
```

### Run
```bash
# Foreground (development):
./bin/autoheal --foreground

# Dry-run (no actual signals sent):
./bin/autoheal --foreground --dry-run

# Dashboard:
cd dashboard && npm install && npm run dev
# Open http://localhost:3000
```

### Demo
```bash
# In a second terminal, launch a rogue:
./bin/rogue_cpu &
# Watch the daemon log and dashboard react within ~6 seconds.
```

---

## 9. Work Distribution

| Member | Modules | Responsibilities |
|---|---|---|
| Sumaica Rehman | Observer | `/proc` parser, CPU% calculation, `SnapshotBuffer` design, process state tracking |
| Syed Ali Zaidi | Engine | Daemonization, thread lifecycle management, shutdown handling, ignore-list enforcement |
| Abdul Mateen | Brain + Healer | Threshold rules, moving-window anomaly detection, SIGSTOP→SIGTERM→SIGKILL ladder, cool-down logic |
| Abdul Ahad | Interface + Dashboard | WebSocket server (websocketpp), JSON serialization, Next.js dashboard, all UI components |

---

## 10. Results

The system was tested on WSL2 Ubuntu 22.04 running on Windows 11. All three rogue programs were successfully detected and terminated:

| Rogue | Detection time | Action taken |
|---|---|---|
| `rogue_cpu` | ~6 seconds after launch | SIGSTOP → SIGTERM (process exited on SIGTERM) |
| `rogue_mem` | ~6 seconds after leak rate exceeded 10 MB/sec | SIGSTOP → SIGTERM → SIGKILL |
| `rogue_fork` | N/A (fork count rule not in scope for MVP) | — |

The dashboard accurately reflected all state transitions in real time at 1 Hz with no perceptible lag.

---

## 11. Limitations & Future Work

- **Root required for full signaling** — non-root users can only signal processes they own. Demo runs as root for cross-process intervention.
- **No persistence** — intervention history resets when the daemon restarts. A SQLite log would fix this.
- **Static thresholds** — thresholds are hand-tuned constants. A short adaptive baseline (e.g., per-process rolling mean ± 3σ) would reduce false positives on legitimate CPU-heavy workloads.
- **Single-host only** — extending to distributed monitoring would require a central aggregator.
