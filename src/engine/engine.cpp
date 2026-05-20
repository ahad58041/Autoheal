// src/engine/engine.cpp
//
// Daemonization via double-fork() + setsid(), then thread orchestration.

#include "engine.hpp"

#include "../observer/observer.hpp"
#include "../brain/brain.hpp"
#include "../healer/healer.hpp"
#include "../interface/ws_server.hpp"
#include "../common/config.hpp"
#include "../common/logger.hpp"

#include <chrono>
#include <csignal>
#include <cstdlib>
#include <fcntl.h>
#include <iostream>
#include <sys/stat.h>
#include <sys/types.h>
#include <thread>
#include <unistd.h>

namespace autoheal {

std::atomic<bool> Engine::shutdown_requested_{false};

namespace {

void handle_signal(int) {
    Engine::request_shutdown();
}

// Standard "double-fork + setsid" daemonization.
// On success the calling process becomes a background daemon detached from
// the controlling terminal. On failure, exits the process.
void daemonize() {
    pid_t pid = fork();
    if (pid < 0) { std::perror("fork#1"); std::exit(1); }
    if (pid > 0) std::exit(0);          // parent goes away

    if (setsid() < 0) { std::perror("setsid"); std::exit(1); }

    // Second fork ensures we can never re-acquire a controlling terminal.
    pid = fork();
    if (pid < 0) { std::perror("fork#2"); std::exit(1); }
    if (pid > 0) std::exit(0);

    umask(0);
    if (chdir("/") < 0) { std::perror("chdir"); }

    // Redirect stdio to /dev/null — daemon shouldn't be writing to terminal.
    int devnull = open("/dev/null", O_RDWR);
    if (devnull >= 0) {
        dup2(devnull, STDIN_FILENO);
        dup2(devnull, STDOUT_FILENO);
        dup2(devnull, STDERR_FILENO);
        if (devnull > 2) close(devnull);
    }
}

}  // namespace

Engine::Engine(EngineOptions opts) : opts_(opts) {}

Engine::~Engine() = default;

void Engine::request_shutdown() {
    shutdown_requested_.store(true);
}

int Engine::run() {
    if (!opts_.foreground) {
        daemonize();
    }

    Logger::instance().init(config::kLogPath, /*mirror_stderr=*/opts_.foreground);
    LOG_INFO("AutoHeal Engine starting (pid=" + std::to_string(getpid()) +
             (opts_.foreground ? ", foreground" : ", daemon") +
             (opts_.dry_run    ? ", dry-run" : "") + ")");

    // Install cooperative shutdown signal handlers.
    struct sigaction sa{};
    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGINT,  &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);
    signal(SIGPIPE, SIG_IGN);

    // Spawn the four workers in dependency order.
    observer_  = std::make_unique<Observer>(buffer_);
    brain_     = std::make_unique<Brain>(buffer_, events_, ignore_);
    healer_    = std::make_unique<Healer>(events_, log_, ignore_, opts_.dry_run);
    interface_ = std::make_unique<WsServer>(buffer_, log_, ignore_, config::kWsPort);

    observer_->start();
    brain_->start();
    healer_->start();
    interface_->start();

    // Idle until shutdown signal arrives.
    while (!shutdown_requested_.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    LOG_INFO("AutoHeal Engine shutting down");

    interface_->stop();
    healer_->stop();
    brain_->stop();
    observer_->stop();

    LOG_INFO("AutoHeal Engine stopped cleanly");
    return 0;
}

}  // namespace autoheal
