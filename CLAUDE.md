# AutoHeal — Project Decisions & Implementation Plan

> **Course:** Operating Systems (folder name `dsa final` is a typo — this is an **OS** project, not DSA).
> **Project:** Self-Healing Process Manager & Anomaly Detector
> **Team (on paper):** Sumaica Rehman, Syed Ali Zaidi, Abdul Mateen, Abdul Ahad
> **Actual builder:** Solo developer. Roles will be assigned to team members at the end.
> **Reference:** See [proj_info.md](proj_info.md) for the original proposal.

---

## 1. Locked Decisions

| Area | Decision | Rationale |
|---|---|---|
| **Dev Environment** | **VirtualBox VM running Ubuntu 22.04** on a Windows 11 host | Full Linux syscalls, `/proc`, signals, pthreads in a fully isolated guest. Work inside `~/autoheal` in the VM's home directory, **not** under `/media/sf_*` shared folders (file I/O is slow and `/proc` watching is awkward there). |
| **Builder** | **Solo** | One person doing everything. Scope and phasing reflect this — no parallel tracks. |
| **MVP Scope** | **Full proposal as written** (all 5 modules) | User-directed. Descope ladder in §7 ready if needed. |
| **Detection Logic** | **Static thresholds + short moving window** | Predictable, demoable, easy to tune live. No statistics or ML overhead. |
| **WebSocket Library** | **websocketpp** (locked) | Header-only, `apt install libwebsocketpp-dev`, mountains of examples, no build pain. |
| **JSON Library** | **nlohmann/json** (locked) | Single header, dead simple. `apt install nlohmann-json3-dev`. |
| **Frontend** | **Next.js 14 + React + TypeScript + Tailwind** | Per proposal. One page consuming WebSocket. |
| **Demo Validation** | **Custom `rogue_*.cpp` test programs** (CPU spinner, RAM leaker, capped fork bomb) | Reproducible demo we fully control. |
| **Run as** | **Non-root user during dev, root only during demo** | Safer — non-root can only signal own processes. Document as a deliberate security stance. |
| **Logging** | **`~/autoheal.log`** (not `/var/log`) | Avoids permission grief. Rotate manually if needed. |
| **Sample Rate** | **1 Hz** (1 snapshot per second) | Tunable in `src/common/config.hpp`. Faster eats CPU; slower hurts demo responsiveness. |

---

## 2. Architecture

```
┌────────────────────┐         ┌────────────────────┐
│  /proc filesystem  │◄────────│   1. OBSERVER      │  (thread A — sampler)
└────────────────────┘         │   parses /proc     │
                               │   emits snapshots  │
                               └─────────┬──────────┘
                                         │ shared SnapshotBuffer
                                         │ (std::mutex protected)
                               ┌─────────▼──────────┐
                               │   3. BRAIN         │  (thread B — analyzer)
                               │   threshold rules  │
                               │   moving window    │
                               └─────────┬──────────┘
                                         │ AnomalyEvent queue
                               ┌─────────▼──────────┐
                               │   4. HEALER        │  (thread C — actor)
                               │   SIGSTOP→SIGTERM  │
                               │   →SIGKILL ladder  │
                               └─────────┬──────────┘
                                         │ InterventionLog
                               ┌─────────▼──────────┐
                               │   5. INTERFACE     │  (thread D — WS server)
                               │   websocketpp :8080│
                               │   JSON broadcast   │
                               └─────────┬──────────┘
                                         │ ws://localhost:8080
                               ┌─────────▼──────────┐
                               │  Next.js Dashboard │  (browser)
                               └────────────────────┘

   ENGINE (2) wraps all of the above: daemonization, thread lifecycle,
   shutdown handling, ignore-list enforcement.
```

**Key invariants:**
- Observer **never blocks** on a single `/proc` read (process can die mid-read → catch & skip).
- `SnapshotBuffer` is single-producer / multi-consumer (Observer writes; Brain + Interface read).
- Healer **never signals** PIDs in the ignore list (PID 1, kthreads, AutoHeal itself, parent shell, login session).
- All strings crossing the WebSocket boundary are JSON-escaped + length-capped.
- The Brain's "rogue" verdict requires **sustained** anomalous behavior (N consecutive samples), never one-shot.

---

## 3. Technology Stack (Locked)

| Layer | Choice | apt package |
|---|---|---|
| Language | C++17 | `g++` (build-essential) |
| Threads | `std::thread` + `std::mutex` (POSIX-backed) | built-in |
| Signals | `<csignal>` `kill(2)` | built-in |
| Build | `Makefile` | `make` (build-essential) |
| WebSocket | **websocketpp** | `libwebsocketpp-dev` |
| WebSocket deps | Boost.Asio | `libboost-all-dev` |
| JSON | **nlohmann/json** | `nlohmann-json3-dev` |
| Frontend | Next.js 14 + TypeScript + Tailwind | via `npx create-next-app` |

---

## 4. VirtualBox Ubuntu Setup — Do This Once

Assumed prerequisite: VirtualBox is installed on Windows 11 and an **Ubuntu 22.04** guest is already created and booting.

### 4a. Get the project files into the VM

Pick one of these (any works):

**Option A — Git (recommended once you have a GitHub repo):**
```bash
# Inside the Ubuntu VM:
cd ~
git clone <your-repo-url> autoheal
cd autoheal
```

**Option B — VirtualBox Shared Folder:**
1. VM powered off: VirtualBox → **Settings → Shared Folders** → add `C:\Users\PMLS\Desktop\final labs\dsa final`, name it `autoheal`, check **Auto-mount** + **Make Permanent**.
2. Boot Ubuntu, add yourself to the `vboxsf` group once, then log out and back in:
   ```bash
   sudo usermod -aG vboxsf $USER
   ```
3. Copy the files into the VM's home dir (do **not** work from `/media/sf_*`):
   ```bash
   cp -r /media/sf_autoheal ~/autoheal
   cd ~/autoheal
   ```

**Option C — Drag-and-drop** (requires Guest Additions installed).

### 4b. Install dependencies (inside the Ubuntu VM)

```bash
sudo apt update && sudo apt upgrade -y
sudo apt install -y build-essential g++ make git \
                    libwebsocketpp-dev libboost-all-dev \
                    nlohmann-json3-dev \
                    nodejs npm \
                    stress-ng htop wscat curl

# Newer Node for Next.js 14:
curl -fsSL https://deb.nodesource.com/setup_20.x | sudo -E bash -
sudo apt install -y nodejs
node --version   # should print v20.x
g++  --version   # should print 11.x or newer
```

### 4c. (Optional) View the dashboard from the Windows host

Easiest: just use **Firefox inside the Ubuntu VM** (`http://localhost:3000`). Done.

If you'd rather browse from Windows, with the VM powered off go to **VirtualBox → Settings → Network → Adapter 1 → Advanced → Port Forwarding** and add:

| Name | Protocol | Host Port | Guest Port |
|---|---|---|---|
| dashboard | TCP | 3000 | 3000 |
| ws        | TCP | 8080 | 8080 |

Then `http://localhost:3000` on the Windows host reaches the VM.

> ⚠️ **Do not** keep the working repo under `/media/sf_*` — file I/O over shared folders is slow and watching `/proc` from a host-mounted path is awkward. Always copy into `~/autoheal` inside the guest.

---

## 5. Implementation Plan — Solo Phased Plan

> Solo realistic budget: **2 focused days** for a working MVP, **3–4 days** for the polished full proposal with report + slides. The hour estimates below assume one person, head-down, no parallelism.

### Phase 0 — Setup (45 min)
- [ ] Boot the VirtualBox Ubuntu 22.04 VM (§4).
- [ ] Install all apt packages inside the VM (§4b).
- [ ] Create GitHub repo, clone inside the VM at `~/autoheal`.
- [ ] Scaffold directory tree (already done — see §6 layout).
- [ ] `make` runs and produces empty binaries linking cleanly.

### Phase 1 — Core Backend (4–5 hrs)
Build in dependency order so each step is testable:

1. **`src/common/snapshot.hpp`** (15 min) — data contract. Lock this first.
2. **`src/observer/observer.cpp`** (1 hr) — `/proc` parser. Test by printing all PIDs + their CPU% / RSS.
3. **`src/brain/brain.cpp`** (1 hr) — rules engine. Test by feeding it fake snapshots and checking it flags expected PIDs.
4. **`src/healer/healer.cpp`** (45 min) — signal ladder. Test by manually injecting an AnomalyEvent and watching `kill -l` confirm signals.
5. **`src/engine/engine.cpp`** + **`src/main.cpp`** (1 hr) — daemonization + thread orchestration. Test by running with `--foreground` flag.
6. **Integration test** (30 min) — run the daemon, launch a rogue, confirm it gets caught and killed.

### Phase 2 — WebSocket Interface (2–3 hrs)
1. **`src/interface/ws_server.cpp`** (1.5 hr) — websocketpp server on port 8080.
2. **JSON serialization** (30 min) — convert SnapshotBuffer + intervention log to JSON every second.
3. **Smoke test with `wscat`** (15 min) — `wscat -c ws://localhost:8080` should print a JSON dump every second.

### Phase 3 — Dashboard (3–4 hrs)
1. **`npx create-next-app@latest dashboard --typescript --tailwind --app`** (15 min)
2. **WebSocket hook** in `app/page.tsx` (30 min)
3. **Process table** — PID, name, CPU%, RAM, sortable (1 hr)
4. **Anomaly banner** + **intervention log** sections (1 hr)
5. **Polish & styling** (1 hr)

### Phase 4 — Rogue Programs (45 min)
1. `rogues/rogue_cpu.cpp` — busy loop.
2. `rogues/rogue_mem.cpp` — bounded leak (cap 1 GB so we don't OOM the VM).
3. `rogues/rogue_fork.cpp` — fork bomb capped at 50 children.

### Phase 5 — Demo Dry-Run + Bug Hunt (2 hrs)
- [ ] Boot daemon → open dashboard → launch each rogue → watch escalation → confirm kill.
- [ ] Tune thresholds so demo is **snappy** (3-second sustained window, not 30).
- [ ] Record GIF of a successful intervention for the report and slides.

### Phase 6 — Report & Slides (3 hrs)
- [ ] `docs/report.md` — problem, architecture (lift §2), each module, detection rules, security, demo screenshots, work distribution (§6).
- [ ] `docs/slides.md` (or .pptx) — ~10 slides.
- [ ] Export both to PDF.

### Phase 7 — Submission (30 min)
- [ ] `make clean && make all` from a fresh clone must work.
- [ ] Tag `v1.0`, push to GitHub.
- [ ] Submit per course portal.

**Realistic total:** ~17–19 hours of focused solo work. Split across **2 long days** or **3 normal days**.

---

## 6. Project Layout (Scaffolded)

```
autoheal/
├── CLAUDE.md              # this file
├── README.md              # build + run instructions
├── Makefile               # builds daemon + rogues
├── proj_info.md           # original proposal
├── src/
│   ├── main.cpp           # entry point
│   ├── common/
│   │   ├── snapshot.hpp   # ProcessSnapshot, AnomalyEvent, Intervention structs
│   │   ├── config.hpp     # all tunable thresholds in one place
│   │   └── ignore_list.hpp
│   ├── observer/
│   │   ├── observer.hpp
│   │   └── observer.cpp   # /proc parser, samples at 1 Hz
│   ├── engine/
│   │   ├── engine.hpp
│   │   └── engine.cpp     # daemonize(), thread lifecycle, shutdown
│   ├── brain/
│   │   ├── brain.hpp
│   │   └── brain.cpp      # threshold rules, moving window
│   ├── healer/
│   │   ├── healer.hpp
│   │   └── healer.cpp     # SIGSTOP → SIGTERM → SIGKILL ladder
│   └── interface/
│       ├── ws_server.hpp
│       └── ws_server.cpp  # websocketpp server, JSON broadcast
├── rogues/
│   ├── rogue_cpu.cpp      # CPU spinner
│   ├── rogue_mem.cpp      # bounded RAM leaker
│   └── rogue_fork.cpp     # capped fork bomb
├── dashboard/             # Next.js app (created in Phase 3)
└── docs/
    ├── report.md
    └── slides/
```

---

## 7. Descope Ladder (cut from the bottom if running out of time)

1. **Cut historical charts / sparklines** → numeric table only in dashboard.
2. **Cut fork-bomb rogue** → demo with CPU + memory rogues only.
3. **Cut `HUNG` (D-state) detection** → keep `CPU_SPIN` + `MEM_LEAK`.
4. **Cut Next.js**, fall back to a single static `dashboard.html` with vanilla JS WebSocket consumer.
5. **Cut WebSocket**, daemon writes JSON lines to `~/autoheal.log`; demo via `tail -f`.
6. **Cut daemonization**, run foreground with `./autoheal --no-daemon`.

**Never cut:** Observer, Brain (CPU rule), Healer (signal ladder), ignore-list safety. Those *are* the project.

---

## 8. Risk Register

| Risk | Likelihood | Mitigation |
|---|---|---|
| Race between Observer read and process termination | **High** | `try { read } catch { skip pid this tick }` — already in plan. |
| Healer signals the wrong PID | **Critical** | Hardcoded ignore list + re-check at signal time + `--dry-run` flag during dev. Never run as root until last hour. |
| Demo rogue crashes the VM | Medium | Fork-bomb capped at 50; test one rogue at a time; **take a VirtualBox snapshot** before the demo so you can revert in one click. |
| websocketpp + Boost install pain | Low | `apt install libwebsocketpp-dev libboost-all-dev` — both are first-class Ubuntu packages. |
| Next.js setup eats hours | Medium | Static HTML fallback is rung 4 of the descope ladder. |
| Thresholds too aggressive → false positives kill legit processes | Medium | All thresholds in `src/common/config.hpp`; tune in Phase 5 dry-run. **Always test as non-root first** so you can't signal anything you don't own. |

---

## 9. Team Role Assignment (fill in at the end)

| Member | Role / Modules | Notes |
|---|---|---|
| Sumaica Rehman | _TBD_ | |
| Syed Ali Zaidi | _TBD_ | |
| Abdul Mateen   | _TBD_ | |
| Abdul Ahad     | _TBD_ | |

> Even though one person is building this, the **report and slides must attribute clear ownership** for the OS-course rubric. Suggested mapping when the time comes:
> - **Observer + /proc parsing** → person comfortable with low-level file I/O
> - **Engine + daemonization + threads** → person comfortable with OS concepts
> - **Brain + Healer + signal ladder** → person comfortable with algorithms / control flow
> - **Interface (WebSocket) + Next.js dashboard** → person comfortable with full-stack

---

## 10. Working With Claude Code In This Repo

- The daemon code lives under `src/`, the frontend under `dashboard/`, rogues under `rogues/`.
- All cross-module data shapes live in `src/common/snapshot.hpp` — change with care.
- Tunable thresholds live in `src/common/config.hpp` — tune there, don't sprinkle magic numbers in module code.
- This file (`CLAUDE.md`) holds the **locked decisions**. If a decision changes, edit here first, then code.
