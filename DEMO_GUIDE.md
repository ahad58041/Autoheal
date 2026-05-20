# AutoHeal — Complete Demo & Run Guide

> This document tells you **exactly** what to install, what to run, what you will see, and how to present the project to an examiner. Follow it top to bottom on a fresh machine and the demo will work.

---

## What This Project Is

AutoHeal is a Linux background daemon written in C++ that:

- Reads `/proc` every second to watch every running process
- Detects rogue behavior (CPU spinning, memory leaks, hung processes)
- Kills the offending process automatically using a signal escalation ladder (SIGSTOP → SIGTERM → SIGKILL)
- Streams everything live to a web dashboard over WebSocket

It has **no external dependencies beyond standard Ubuntu packages** — no Docker, no cloud, no subscription. Everything runs locally inside WSL2.

---

## Part 1 — One-Time Machine Setup

> Skip this part if WSL2 + Ubuntu is already installed on the machine.

### Step 1 — Install WSL2 (Windows 11)

Open **PowerShell as Administrator** and run:

```powershell
wsl --install -d Ubuntu-22.04
```

This downloads and installs Ubuntu 22.04 with the WSL2 kernel. **Reboot when prompted.**

After reboot, open the **Ubuntu** app from the Start menu. It will:
- Finish setup (takes 1–2 minutes)
- Ask you to create a Linux username (lowercase, e.g. `ahad`)
- Ask you to set a password (characters won't show while typing — that is normal)

Verify WSL2 is active (from PowerShell):

```powershell
wsl -l -v
```

You should see `VERSION 2` next to Ubuntu-22.04. If it says `1`:

```powershell
wsl --set-version Ubuntu-22.04 2
```

---

### Step 2 — Install Project Dependencies (inside Ubuntu)

Open the Ubuntu terminal and run all of this at once:

```bash
sudo apt update && sudo apt upgrade -y

sudo apt install -y build-essential g++ make git \
                    libwebsocketpp-dev libboost-all-dev \
                    nlohmann-json3-dev wscat

curl -fsSL https://deb.nodesource.com/setup_20.x | sudo -E bash -
sudo apt install -y nodejs
```

Verify the tools installed correctly:

```bash
g++ --version       # should print: g++ (Ubuntu ...) 11.x or newer
node --version      # should print: v20.x
```

---

### Step 3 — Clone the Repository

> Important: clone **inside** the WSL filesystem, NOT under `/mnt/c/...`. The Windows-mounted path is slow and `/proc` monitoring behaves oddly there.

```bash
cd ~
git clone https://github.com/ahad58041/Autoheal.git autoheal
cd autoheal
```

---

### Step 4 — Build Everything

```bash
make clean && make
```

Expected output (all lines should end without errors):

```
g++ ... -c -o build/src/main.o src/main.cpp
g++ ... -c -o build/src/engine/engine.o src/engine/engine.cpp
g++ ... -c -o build/src/observer/observer.o src/observer/observer.cpp
g++ ... -c -o build/src/brain/brain.o src/brain/brain.cpp
g++ ... -c -o build/src/healer/healer.o src/healer/healer.cpp
g++ ... -c -o build/src/interface/ws_server.o src/interface/ws_server.cpp
g++ ... -c -o build/src/interface/json_serializer.o src/interface/json_serializer.cpp
g++ ... -c -o build/src/common/snapshot.o src/common/snapshot.cpp
g++ ... -c -o build/src/common/ignore_list.o src/common/ignore_list.cpp
g++ ... -c -o build/src/common/logger.o src/common/logger.cpp
g++ ... -o bin/autoheal ...
g++ ... -o bin/rogue_cpu rogues/rogue_cpu.cpp
g++ ... -o bin/rogue_mem rogues/rogue_mem.cpp
g++ ... -o bin/rogue_fork rogues/rogue_fork.cpp
```

Confirm the binaries exist:

```bash
ls bin/
# autoheal  rogue_cpu  rogue_mem  rogue_fork
```

---

### Step 5 — Install Dashboard Dependencies

```bash
cd ~/autoheal/dashboard
npm install
cd ~/autoheal
```

This takes about 1–2 minutes. You will see some security warnings — ignore them, they do not affect the demo.

> **Important:** Do NOT run `npm audit fix --force` — it will break the dashboard.

---

## Part 2 — Running the Project

You need **three terminal windows** open. In Windows, open Ubuntu from the Start menu three times, or use Windows Terminal with three tabs. All three should start in `~/autoheal`:

```bash
cd ~/autoheal   # run this in each terminal
```

---

### Terminal 1 — Start the Daemon

```bash
./bin/autoheal --foreground
```

**What you will see:**

```
2026-05-21 00:05:15 [INFO] AutoHeal Engine starting (pid=613, foreground)
2026-05-21 00:05:15 [INFO] Observer: started
2026-05-21 00:05:15 [INFO] Brain: started
2026-05-21 00:05:15 [INFO] Healer: started
2026-05-21 00:05:15 [INFO] WsServer: listening on port 8080
```

The daemon is now running and watching every process on the system. It will stay here printing logs. **Do not close this terminal.**

---

### Terminal 2 — Start the Dashboard

```bash
cd ~/autoheal/dashboard
npm run dev
```

**What you will see:**

```
▲ Next.js 14.2.5
- Local: http://localhost:3000
✓ Ready in 2.4s
```

Now open **any browser on Windows** and go to:

```
http://localhost:3000
```

**What you will see in the browser:**

- A dark dashboard titled **AutoHeal**
- A green dot labeled **Connected** in the top-right corner
- A green bar: *"✓ System healthy — no active anomalies."*
- A **Live Processes** table filling up with every running process (PID, name, CPU%, RAM, state)
- An empty **Intervention Log** at the bottom

The process table updates every second in real time.

---

### Terminal 3 — Launch a Rogue Program

This is the demo. Run one rogue at a time.

#### Demo 1 — CPU Spinner

```bash
./bin/rogue_cpu &
```

The rogue prints:
```
rogue_cpu (pid=XXXX) — spinning forever. Ctrl+C to stop.
```

**What happens next (watch Terminal 1 and the browser):**

| Time | Terminal 1 log | Browser |
|------|----------------|---------|
| 0–5 sec | Silent (Brain is counting samples) | `rogue_cpu` row appears, CPU% climbs to ~100%, highlighted in red |
| ~6 sec | `[WARN] Brain flag CPU_SPIN pid=XXXX comm=rogue_cpu` | Anomaly banner turns red: "⚠ 1 active anomaly: rogue_cpu(XXXX) CPU_SPIN" |
| ~6 sec | `[INFO] Healer SIGSTOP pid=XXXX comm=rogue_cpu` | Intervention Log shows SIGSTOP entry |
| ~9 sec | `[INFO] Healer SIGTERM pid=XXXX comm=rogue_cpu` | Intervention Log shows SIGTERM entry |
| ~12 sec | `[WARN] Healer SIGKILL pid=XXXX comm=rogue_cpu` (if not dead yet) | Intervention Log shows SIGKILL entry |
| Done | Process gone | `rogue_cpu` row disappears, banner returns green |

In Terminal 3 you will also see:
```
[1]+  Terminated    ./bin/rogue_cpu
```

That confirms the process was killed by the signal.

---

#### Demo 2 — Memory Leaker

```bash
./bin/rogue_mem &
```

The rogue leaks ~50 MB per second. The Brain detects when RSS growth exceeds 10 MB/sec sustained for 5 seconds.

**What you will see in the browser:** the `rogue_mem` row's **RSS (MB)** column climbs rapidly. After ~6 seconds the anomaly banner fires with `MEM_LEAK` and the Healer kills it the same way.

---

#### Demo 3 — Fork Bomb (capped)

```bash
./bin/rogue_fork &
```

This forks up to 50 child processes then stops. You will see many `rogue_fork` rows appear in the process table simultaneously.

> Note: fork-count monitoring is not in the MVP detection rules. This rogue is included to show the process-table populating with many entries at once — it demonstrates the Observer and dashboard working at scale.

---

## Part 3 — How to Present This to an Examiner

### Suggested Demo Script (5 minutes)

**1. Show the architecture (30 seconds)**  
Point to the terminal: *"The daemon has five threads. Observer reads `/proc` every second. Brain applies detection rules. Healer sends signals. Interface broadcasts JSON over WebSocket. The browser renders it live."*

**2. Show the idle dashboard (30 seconds)**  
Open `http://localhost:3000`. Point out: *"Green dot means WebSocket is connected. The process table is live — these are actual system processes with real CPU and RAM numbers updating every second."*

**3. Run rogue_cpu and narrate (2 minutes)**  
```bash
./bin/rogue_cpu &
```
While waiting: *"The Brain requires sustained anomalous behavior — it won't act on a single spike. It waits for 5 consecutive samples above 90% CPU."*

When the banner fires: *"There — Brain flagged CPU_SPIN. Healer is now executing the escalation ladder."*

Point to the Intervention Log: *"SIGSTOP froze the process. SIGTERM asked it to exit. If it ignored that, SIGKILL would force-terminate it."*

When the rogue disappears: *"Process is gone. Banner returns green. System is healthy."*

**4. Run rogue_mem (1 minute)**  
```bash
./bin/rogue_mem &
```
*"Same pipeline, different rule. This one detects RSS growing faster than 10 MB per second. Watch the RAM column."*

**5. Answer likely questions**

| Question | Answer |
|----------|--------|
| "Why not just use top or htop?" | Those only monitor — they don't act. AutoHeal closes the feedback loop automatically. |
| "What stops it from killing important processes?" | Hardcoded ignore list (PID 1, kernel threads, its own PID, your shell). Also re-checks at signal time in case a PID was recycled. |
| "What is SIGSTOP for?" | It freezes the process instantly (no CPU consumed) while the daemon decides what to do next. It's safer than jumping straight to SIGKILL. |
| "What if SIGTERM is enough?" | Then SIGKILL never fires. The ladder stops as soon as the process exits. |
| "How does it detect memory leaks?" | It reads `VmRSS` from `/proc/<pid>/status` across a 5-second window and computes growth rate. If growth > 10 MB/sec sustained, it flags MEM_LEAK. |
| "Why WebSocket and not just a log file?" | Real-time push with no polling. The browser receives every update within 1 second of it happening. |

---

## Part 4 — Stopping Everything

**Stop the rogue** (if still running):
```bash
killall rogue_cpu
# or:
killall rogue_mem
```

**Stop the daemon** (Terminal 1):
```
Ctrl+C
```

**Stop the dashboard** (Terminal 2):
```
Ctrl+C
```

---

## Part 5 — Troubleshooting

### "No such file or directory" when running ./bin/autoheal
You are in the wrong directory. Run:
```bash
cd ~/autoheal
```

### Dashboard shows "Disconnected"
The daemon is not running. Start it first:
```bash
./bin/autoheal --foreground
```

### npm install gets killed / Terminated
AutoHeal is running and is killing npm because it used too much CPU. Stop the daemon (Ctrl+C in Terminal 1), run `npm install`, then restart the daemon.

### make fails with "websocketpp not found"
```bash
sudo apt install -y libwebsocketpp-dev libboost-all-dev
```

### make fails with "nlohmann/json.hpp not found"
```bash
sudo apt install -y nlohmann-json3-dev
```

### Rogue runs but daemon never flags it
Check that you are running from inside `~/autoheal`, not `/mnt/c/...`. Also confirm the daemon printed the five startup lines. If the rogue exits before 5 seconds, the Brain's window never fills — let it run longer.

### WSL2 is completely frozen
From PowerShell on Windows:
```powershell
wsl --shutdown
```
Then re-open Ubuntu normally. This is safe — WSL2 has no persistent state.

---

## Part 6 — Quick Reference Card

| What | Command |
|------|---------|
| Build everything | `make clean && make` |
| Start daemon | `./bin/autoheal --foreground` |
| Start daemon (safe/no signals) | `./bin/autoheal --foreground --dry-run` |
| Start dashboard | `cd dashboard && npm run dev` |
| Open dashboard | Browser → `http://localhost:3000` |
| Launch CPU rogue | `./bin/rogue_cpu &` |
| Launch memory rogue | `./bin/rogue_mem &` |
| Launch fork rogue | `./bin/rogue_fork &` |
| Kill all rogues | `killall rogue_cpu rogue_mem rogue_fork` |
| View live log | `tail -f ~/autoheal.log` |
| Stop daemon | `Ctrl+C` in Terminal 1 |
| Stop dashboard | `Ctrl+C` in Terminal 2 |
| Emergency WSL reset | PowerShell: `wsl --shutdown` |
