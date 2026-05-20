// src/brain/brain.hpp
//
// The Brain reads ProcessTable history from SnapshotBuffer, applies
// threshold rules over a moving window, and emits AnomalyEvents into EventQueue.

#pragma once

#include "../common/snapshot.hpp"
#include "../common/ignore_list.hpp"
#include <atomic>
#include <thread>
#include <unordered_map>

namespace autoheal {

class Brain {
public:
    Brain(SnapshotBuffer& buffer, EventQueue& events, const IgnoreList& ignore);
    ~Brain();

    void start();
    void stop();

private:
    void run_();
    void evaluate_();

    // Per-PID rolling memory of how long it's been suspicious.
    struct Track {
        size_t cpu_high_streak  = 0;
        size_t hung_streak      = 0;
        uint64_t baseline_rss_kb = 0;
        TimePoint baseline_at;
        TimePoint last_flagged_at;
        bool in_cooldown        = false;
    };

    SnapshotBuffer&   buffer_;
    EventQueue&       events_;
    const IgnoreList& ignore_;

    std::unordered_map<int, Track> tracks_;
    std::atomic<bool> running_{false};
    std::thread thread_;
};

}  // namespace autoheal
