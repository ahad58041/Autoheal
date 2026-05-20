// src/main.cpp
//
// CLI front-door for the AutoHeal daemon.
//
// Usage:
//   autoheal                # daemonize and run
//   autoheal --foreground   # stay attached to the terminal (good for demos)
//   autoheal --dry-run      # never actually send kill signals (safe testing)
//   autoheal --help

#include "engine/engine.hpp"

#include <cstring>
#include <iostream>

namespace {

void print_help() {
    std::cout <<
        "AutoHeal — Self-Healing Process Manager & Anomaly Detector\n"
        "Usage:\n"
        "  autoheal [--foreground] [--dry-run] [--help]\n"
        "\n"
        "  --foreground   Do not daemonize; keep logs on stderr.\n"
        "  --dry-run      Detect and log, but do not signal any process.\n"
        "  --help         Show this message.\n";
}

}  // namespace

int main(int argc, char** argv) {
    autoheal::EngineOptions opts;
    for (int i = 1; i < argc; ++i) {
        if      (std::strcmp(argv[i], "--foreground") == 0) opts.foreground = true;
        else if (std::strcmp(argv[i], "--dry-run")    == 0) opts.dry_run    = true;
        else if (std::strcmp(argv[i], "--help")       == 0) { print_help(); return 0; }
        else {
            std::cerr << "Unknown argument: " << argv[i] << "\n";
            print_help();
            return 2;
        }
    }

    autoheal::Engine engine(opts);
    return engine.run();
}
