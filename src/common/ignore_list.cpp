// src/common/ignore_list.cpp

#include "ignore_list.hpp"
#include <unistd.h>

namespace autoheal {

// Process names that must never be killed — our own dashboard and Node.js tooling.
const std::unordered_set<std::string> IgnoreList::hard_ignored_comms_ = {
    "next-server",   // Next.js dashboard (our own UI)
    "node",          // Node.js runtime
    "npm",           // npm scripts
    "sh",            // shell scripts spawned by npm
    "bash",          // shells
};

IgnoreList::IgnoreList() {
    hard_ignored_pids_.insert(1);              // init / systemd
    hard_ignored_pids_.insert(2);              // kthreadd
    hard_ignored_pids_.insert(getpid());       // ourselves
    hard_ignored_pids_.insert(getppid());      // parent shell / launcher
    hard_ignored_pids_.insert(getsid(0));      // session leader
}

void IgnoreList::add(int pid) {
    hard_ignored_pids_.insert(pid);
}

bool IgnoreList::should_protect(const ProcessSnapshot& p) const {
    if (hard_ignored_pids_.count(p.pid)) return true;
    if (hard_ignored_comms_.count(p.comm)) return true;

    // Anything reparented to kthreadd (PID 2) is a kernel thread.
    if (p.ppid == 2) return true;

    // Kernel threads have comm in brackets, e.g. "[ksoftirqd/0]".
    if (!p.comm.empty() && p.comm.front() == '[' && p.comm.back() == ']') return true;

    // Defensive: PID 0 is the kernel scheduler — should never appear in /proc anyway.
    if (p.pid <= 0) return true;

    return false;
}

}  // namespace autoheal
