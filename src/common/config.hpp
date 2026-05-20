// src/common/config.hpp
//
// All tunable thresholds in one place. Tune here, never inline in module code.

#pragma once

#include <cstddef>
#include <string>

namespace autoheal::config {

// --- Observer ---
inline constexpr int    kSampleIntervalMs   = 1000;          // 1 Hz sampling
inline constexpr size_t kMaxProcessNameLen  = 64;            // truncate comm to this length

// --- Brain ---
// Window over which "sustained" behavior is measured. At 1 Hz, this many samples.
inline constexpr size_t kWindowSamples       = 5;            // 5 seconds at 1 Hz

// CPU_SPIN rule: CPU% averaged over kWindowSamples must exceed this.
inline constexpr double kCpuSpinPctThreshold = 90.0;

// MEM_LEAK rule: RSS must grow by at least this KB per second, sustained.
inline constexpr uint64_t kMemLeakKbPerSecThreshold = 10 * 1024;   // 10 MB/sec

// HUNG rule: process in D-state this many consecutive samples.
inline constexpr size_t kHungSamples = 8;

// Cool-down: do not re-flag the same PID within this many seconds after action taken.
inline constexpr int kCooldownSeconds = 5;

// --- Healer ---
// Grace period between each rung of the escalation ladder (seconds).
inline constexpr int kGraceSigstopToTermSec = 3;
inline constexpr int kGraceSigtermToKillSec = 3;

// --- Interface ---
inline constexpr int kWsPort           = 8080;
inline constexpr int kBroadcastEveryMs = 1000;     // push JSON every 1s

// --- Logging ---
inline const std::string kLogPath = std::string(getenv("HOME") ? getenv("HOME") : "/tmp") + "/autoheal.log";

}  // namespace autoheal::config
