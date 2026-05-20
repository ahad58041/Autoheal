// src/healer/healer.hpp
//
// The Healer drains AnomalyEvents and applies a tiered signal ladder:
//   SIGSTOP  → if still rogue after grace → SIGTERM → if still alive → SIGKILL.
//
// Every signal call is gated by IgnoreList::should_protect() — double-checked
// at the moment of action, not just when the event was created.

#pragma once

#include "../common/snapshot.hpp"
#include "../common/ignore_list.hpp"
#include <atomic>
#include <thread>
#include <unordered_map>

namespace autoheal {

class Healer {
public:
    Healer(EventQueue& events,
           InterventionLog& log,
           const IgnoreList& ignore,
           bool dry_run = false);
    ~Healer();

    void start();
    void stop();

private:
    void run_();
    void process_event_(const AnomalyEvent& ev);
    void escalate_();

    // True if a process with this pid is still alive (`kill(pid, 0)`).
    static bool process_alive(int pid);

    // Append a row to the InterventionLog (thread-safe).
    void record_(const Intervention& iv);

    struct Pending {
        AnomalyEvent ev;
        HealStage   stage;
        TimePoint    stage_entered_at;
    };

    EventQueue&       events_;
    InterventionLog&  log_;
    const IgnoreList& ignore_;
    bool              dry_run_;

    std::unordered_map<int, Pending> in_flight_;     // pid -> escalation state
    std::atomic<bool> running_{false};
    std::thread       thread_;
};

}  // namespace autoheal
