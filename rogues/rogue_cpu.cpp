// rogues/rogue_cpu.cpp
//
// CPU spinner — burns one core to near-100% indefinitely.
// AutoHeal should flag this with CPU_SPIN and escalate to SIGKILL.
//
// Build: make rogues
// Run  : ./bin/rogue_cpu

#include <cstdio>
#include <unistd.h>

int main() {
    std::printf("rogue_cpu (pid=%d) — spinning forever. Ctrl+C to stop.\n", getpid());
    std::fflush(stdout);

    // Volatile so the compiler can't optimize the loop away.
    volatile unsigned long counter = 0;
    while (true) {
        counter++;
    }
    return 0;
}
