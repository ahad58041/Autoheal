// src/engine/engine.hpp
//
// The Engine owns lifecycle for the whole daemon: optional daemonization,
// signal handling, and the four worker threads (Observer / Brain / Healer /
// Interface).

#pragma once

#include "../common/snapshot.hpp"
#include "../common/ignore_list.hpp"

#include <atomic>
#include <memory>

namespace autoheal {

class Observer;
class Brain;
class Healer;
class WsServer;

struct EngineOptions {
    bool foreground = false;     // skip daemonize()
    bool dry_run    = false;     // Healer logs but doesn't actually kill
};

class Engine {
public:
    explicit Engine(EngineOptions opts);
    ~Engine();

    // Daemonize (if requested), start threads, install signal handlers,
    // and block until SIGINT or SIGTERM is received. Returns the exit code.
    int run();

    // Cooperative shutdown — usually invoked from a signal handler.
    static void request_shutdown();

private:
    EngineOptions opts_;

    SnapshotBuffer  buffer_;
    EventQueue      events_;
    InterventionLog log_;
    IgnoreList      ignore_;

    std::unique_ptr<Observer> observer_;
    std::unique_ptr<Brain>    brain_;
    std::unique_ptr<Healer>   healer_;
    std::unique_ptr<WsServer> interface_;

    static std::atomic<bool> shutdown_requested_;
};

}  // namespace autoheal
