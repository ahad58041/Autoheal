// src/common/ignore_list.hpp
//
// Hardcoded safe-list. Healer MUST consult this before signaling any PID.
// This is the single most important safety mechanism in AutoHeal.

#pragma once

#include "snapshot.hpp"
#include <string>
#include <unordered_set>

namespace autoheal {

class IgnoreList {
public:
    // Initialize with our own PID and parent PID so we can never kill ourselves
    // or the shell that launched us.
    IgnoreList();

    // True if signalling this pid would be dangerous.
    // Checks: PID 1 (init), PID 2 (kthreadd), our own pid, our parent, login session.
    // Also recognizes kernel threads (PPID == 2 OR comm wrapped in brackets).
    bool should_protect(const ProcessSnapshot& p) const;

    // Add a PID at runtime (e.g. system services we discover).
    void add(int pid);

private:
    std::unordered_set<int>         hard_ignored_pids_;
    // Process names (comm) that should never be signalled.
    static const std::unordered_set<std::string> hard_ignored_comms_;
};

}  // namespace autoheal
