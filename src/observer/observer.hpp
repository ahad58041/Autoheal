// src/observer/observer.hpp
//
// The Observer parses /proc and emits ProcessTable snapshots into a SnapshotBuffer.
// Runs as its own thread, sampling at config::kSampleIntervalMs (1 Hz).

#pragma once

#include "../common/snapshot.hpp"
#include <atomic>
#include <optional>
#include <thread>

namespace autoheal {

class Observer {
public:
    explicit Observer(SnapshotBuffer& buffer);
    ~Observer();

    void start();   // spawns sampler thread
    void stop();    // signals thread to exit and joins

    // Exposed for unit testing — parses one snapshot from raw stat/status strings.
    static std::optional<ProcessSnapshot> parse_one(int pid);

private:
    void run_();    // thread body
    ProcessTable sweep_();

    SnapshotBuffer& buffer_;
    std::atomic<bool> running_{false};
    std::thread       thread_;
};

}  // namespace autoheal
