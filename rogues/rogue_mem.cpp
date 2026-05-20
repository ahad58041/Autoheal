// rogues/rogue_mem.cpp
//
// Memory leaker — allocates 16 MB every 250 ms and touches every page
// so the kernel actually backs the allocation with RSS, not just commit.
// Bounded at 1 GB so we don't OOM the WSL VM during the demo.
//
// Build: make rogues
// Run  : ./bin/rogue_mem

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <vector>
#include <unistd.h>

int main() {
    std::printf("rogue_mem (pid=%d) — growing RSS in 16 MB steps (cap 1 GB).\n", getpid());
    std::fflush(stdout);

    constexpr size_t kChunk = 16ULL * 1024 * 1024;     // 16 MB
    constexpr size_t kCapMB = 1024;                    // 1 GB cap
    std::vector<char*> holdings;

    size_t allocated_mb = 0;
    while (allocated_mb < kCapMB) {
        char* block = static_cast<char*>(std::malloc(kChunk));
        if (!block) {
            std::fprintf(stderr, "malloc returned NULL at %zu MB\n", allocated_mb);
            break;
        }
        // Touch every page to force RSS growth.
        std::memset(block, 0xAA, kChunk);
        holdings.push_back(block);
        allocated_mb += kChunk / (1024 * 1024);

        std::printf("  rogue_mem now holding %zu MB\n", allocated_mb);
        std::fflush(stdout);
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }

    std::printf("rogue_mem hit cap, holding %zu MB indefinitely\n", allocated_mb);
    while (true) std::this_thread::sleep_for(std::chrono::seconds(60));
    return 0;
}
