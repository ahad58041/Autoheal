# AutoHeal — Self-Healing Process Manager & Anomaly Detector

> **Course:** Operating Systems final project
> *(folder is named `dsa final` due to a typo; this is an OS project, not DSA.)*

AutoHeal is a low-level C++ background daemon for Linux that:

1. **Monitors** every process on the system by parsing `/proc` directly.
2. **Detects** rogue behavior — CPU spinning, memory leaks, D-state hangs — using sustained-threshold rules over a short moving window.
3. **Heals** the system autonomously with a tiered POSIX-signal escalation ladder (`SIGSTOP → SIGTERM → SIGKILL`).
4. **Visualizes** everything in real time via a Next.js + Tailwind dashboard fed by a C++ `websocketpp` server.

See [proj_info.md](proj_info.md) for the full proposal and [CLAUDE.md](CLAUDE.md) for locked design decisions and the phased plan.

---

## Architecture

```
┌──────────────┐  /proc   ┌──────────┐ snapshots ┌────────┐ events ┌────────┐ kill() ┌───────────┐
│   Linux OS   │ ──────▶  │ OBSERVER │ ────────▶ │ BRAIN  │ ─────▶ │ HEALER │ ─────▶ │  Process  │
└──────────────┘          └──────────┘           └────────┘        └───┬────┘        └───────────┘
                                                                       │ log
                                                                       ▼
                              ┌──────────────────────────────────────────┐
                              │ ENGINE (daemon + thread orchestration)   │
                              └──────────────────┬───────────────────────┘
                                                 │ JSON
                                                 ▼
                              ┌──────────────────────────────────────────┐
                              │ INTERFACE: websocketpp :8080  ──▶  Next.js│
                              └──────────────────────────────────────────┘
```

Modules live under `src/` with one folder per concern:

| Folder | Module |
|---|---|
| `src/observer/`  | `/proc` parser (Observer thread) |
| `src/brain/`     | Anomaly detection rules (Brain thread) |
| `src/healer/`    | POSIX signal escalation (Healer thread) |
| `src/interface/` | `websocketpp` server + JSON serializer (Interface thread) |
| `src/engine/`    | Daemonization + thread lifecycle (Engine) |
| `src/common/`    | Shared data contract: `snapshot.hpp`, `config.hpp`, `ignore_list.hpp`, `logger.hpp` |
| `rogues/`        | Test programs (CPU spinner / mem leaker / capped fork bomb) |
| `dashboard/`     | Next.js 14 + Tailwind dashboard |

---

## Build (inside WSL2 Ubuntu)

```bash
# 1. Install dependencies (one-time)
sudo apt update
sudo apt install -y build-essential g++ make git \
                    libwebsocketpp-dev libboost-all-dev \
                    nlohmann-json3-dev wscat

# 2. Newer Node.js for the dashboard
curl -fsSL https://deb.nodesource.com/setup_20.x | sudo -E bash -
sudo apt install -y nodejs

# 3. Build the daemon + rogue programs
make             # produces bin/autoheal, bin/rogue_cpu, bin/rogue_mem, bin/rogue_fork

# 4. Install dashboard dependencies (one-time)
cd dashboard
npm install
cd ..
```

> ⚠️ Do **not** clone this repo under `/mnt/c/...`. Clone inside the WSL filesystem (e.g. `~/autoheal`). File I/O on `/mnt/c` is ~10× slower and watching `/proc` from a Windows-mounted path is awkward.

---

## Run the demo

Open **three** terminals in WSL (just open "Ubuntu" from Start three times).

**Terminal 1 — start the daemon in foreground mode:**
```bash
./bin/autoheal --foreground
```
Logs stream to stderr and to `~/autoheal.log`.

**Terminal 2 — start the dashboard:**
```bash
cd dashboard
npm run dev
# Open http://localhost:3000 in your browser (works from Windows side too)
```

**Terminal 3 — launch a rogue and watch AutoHeal catch it:**
```bash
./bin/rogue_cpu     # or rogue_mem, or rogue_fork
```

Within a few seconds you should see:
1. The rogue appears in the **Live Processes** table.
2. Its CPU% (or RSS) climbs past the threshold.
3. The row turns red and the **Anomaly Banner** fires.
4. The **Intervention Log** records `SIGSTOP → SIGTERM → SIGKILL`.
5. The rogue process disappears.

### Safer testing: dry-run mode

```bash
./bin/autoheal --foreground --dry-run
```
The daemon detects anomalies and writes intervention log entries, but never actually sends kill signals. Use this until you trust the thresholds.

---

## Configuration

All tunable thresholds live in [`src/common/config.hpp`](src/common/config.hpp):

| Constant | Meaning |
|---|---|
| `kSampleIntervalMs` | how often Observer sweeps `/proc` (default 1000ms) |
| `kWindowSamples`    | how many samples count as "sustained" (default 5) |
| `kCpuSpinPctThreshold` | CPU% above which we start counting toward CPU_SPIN |
| `kMemLeakKbPerSecThreshold` | RSS growth rate that counts as a leak |
| `kHungSamples`      | D-state samples before flagging HUNG |
| `kGraceSigstopToTermSec`, `kGraceSigtermToKillSec` | escalation timing |
| `kWsPort`           | WebSocket port the dashboard connects to (default 8080) |

Tune here, never inline in module code.

---

## Safety design

Because AutoHeal kills processes, several layers prevent friendly-fire:

1. **Hardcoded ignore list** ([`src/common/ignore_list.hpp`](src/common/ignore_list.hpp)) — PID 1, PID 2 (`kthreadd`), the daemon's own PID, its parent shell, session leader, and anything detected as a kernel thread.
2. **Re-check at signal time** — the Healer queries the ignore list again at the moment it would call `kill()`, not just when the event was first created.
3. **Run as non-root during development** — POSIX permissions restrict `kill()` to your own processes anyway.
4. **`--dry-run` flag** — exercise the entire pipeline without sending signals.
5. **Sustained-window detection** — a process must remain anomalous for multiple consecutive samples, never one tick.
6. **Cool-down per PID** — after acting on a PID we suppress re-flagging for a configurable window so we don't pile signals on top of each other.

---

## Project layout

```
.
├── CLAUDE.md               # locked decisions & phased plan
├── proj_info.md            # original proposal
├── README.md               # this file
├── Makefile                # builds daemon + rogues
├── src/
│   ├── main.cpp
│   ├── common/   (snapshot, config, ignore_list, logger)
│   ├── observer/ (proc parser)
│   ├── engine/   (daemonization, thread orchestration)
│   ├── brain/    (anomaly rules)
│   ├── healer/   (signal escalation)
│   └── interface/(websocketpp server + JSON serializer)
├── rogues/
│   ├── rogue_cpu.cpp
│   ├── rogue_mem.cpp
│   └── rogue_fork.cpp
├── dashboard/              # Next.js 14 + Tailwind
│   ├── app/
│   ├── components/
│   └── lib/
└── docs/                   # report + slides go here
```

---

## License

Course project — see your instructor for use restrictions.
