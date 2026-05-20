// rogues/rogue_fork.cpp
//
// Capped fork bomb — spawns up to 50 children that each busy-spin.
// Hard cap is critical: an uncapped fork bomb will lock the VM.
// AutoHeal should catch the children (CPU_SPIN) and kill them.
//
// Build: make rogues
// Run  : ./bin/rogue_fork

#include <chrono>
#include <cstdio>
#include <thread>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

constexpr int kMaxChildren = 50;

[[noreturn]] void child_work() {
    volatile unsigned long c = 0;
    while (true) c++;
}

int main() {
    std::printf("rogue_fork (pid=%d) — spawning up to %d children.\n",
                getpid(), kMaxChildren);
    std::fflush(stdout);

    int spawned = 0;
    while (spawned < kMaxChildren) {
        pid_t pid = fork();
        if (pid < 0) {
            std::perror("fork");
            break;
        }
        if (pid == 0) {
            child_work();   // never returns
        }
        spawned++;
        std::printf("  spawned child #%d (pid=%d)\n", spawned, pid);
        std::fflush(stdout);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::printf("rogue_fork: reached cap, waiting for children to die.\n");
    while (wait(nullptr) > 0) { /* reap */ }
    return 0;
}
