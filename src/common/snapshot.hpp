// src/common/snapshot.hpp
//
// Cross-module data contract for AutoHeal.
// Every module reads or writes one of these structs — change with care.

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <chrono>
#include <mutex>
#include <deque>
#include <unordered_map>

namespace autoheal {

// Wall-clock timestamp used everywhere.
using TimePoint = std::chrono::steady_clock::time_point;

// One sample of a single process at a single instant.
// Produced by Observer, consumed by Brain and Interface.
struct ProcessSnapshot {
    int pid              = -1;
    int ppid             = -1;
    std::string comm;             // process name from /proc/[pid]/comm or stat
    char state           = '?';   // R=run, S=sleep, D=uninterruptible, Z=zombie, T=stopped
    uint64_t utime_ticks = 0;     // user CPU ticks (clock ticks, see _SC_CLK_TCK)
    uint64_t stime_ticks = 0;     // system CPU ticks
    uint64_t rss_kb      = 0;     // resident set size in KB (from VmRSS)
    uint64_t vsize_kb    = 0;     // virtual memory size in KB
    int      uid         = -1;    // owning user id
    uint64_t starttime   = 0;     // process start time in ticks since boot
    TimePoint sampled_at;          // when we took this sample

    // Computed by Brain comparing two consecutive snapshots.
    // Not filled by Observer.
    double cpu_pct_instant = 0.0;
};

// One full sweep of /proc — every process the Observer saw this tick.
struct ProcessTable {
    TimePoint sampled_at;
    std::vector<ProcessSnapshot> processes;
};

// Reason a process was flagged as rogue.
enum class AnomalyType {
    CPU_SPIN,    // sustained near-100% CPU
    MEM_LEAK,    // monotonically growing RSS
    HUNG,        // stuck in D-state too long
};

const char* to_string(AnomalyType t);

// Emitted by Brain, consumed by Healer.
struct AnomalyEvent {
    int          pid;
    std::string  comm;
    AnomalyType  type;
    std::string  detail;       // human-readable e.g. "98.2% CPU for 5 samples"
    TimePoint    first_seen;
    TimePoint    flagged_at;
};

// Where a misbehaving PID sits in the escalation ladder.
enum class HealStage {
    OBSERVED,    // flagged but not yet acted on
    STOPPED,     // SIGSTOP sent
    TERMINATED,  // SIGTERM sent
    KILLED,      // SIGKILL sent
    RESOLVED,    // process is gone
};

const char* to_string(HealStage s);

// One row in the intervention log shown on the dashboard.
struct Intervention {
    int          pid;
    std::string  comm;
    AnomalyType  reason;
    HealStage    stage;
    int          signal_sent;      // 0 if none this row
    std::string  outcome;          // "process exited", "ignored", "still running" ...
    TimePoint    at;
};

// Shared between Observer (writer) and Brain/Interface (readers).
// Single-producer / multi-consumer. Protect with snapshot_mu.
struct SnapshotBuffer {
    mutable std::mutex mu;
    ProcessTable latest;                         // most recent full sweep
    std::deque<ProcessTable> history;            // last N sweeps (for moving window)
    static constexpr size_t kMaxHistory = 30;    // 30 seconds at 1 Hz
};

// Shared between Brain (writer) and Healer + Interface (readers).
struct EventQueue {
    mutable std::mutex mu;
    std::deque<AnomalyEvent> pending;            // Healer drains these
};

// Shared between Healer (writer) and Interface (reader). Append-only.
struct InterventionLog {
    mutable std::mutex mu;
    std::deque<Intervention> entries;
    static constexpr size_t kMaxEntries = 200;
};

}  // namespace autoheal
