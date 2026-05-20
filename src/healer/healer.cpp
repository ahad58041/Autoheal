// src/healer/healer.cpp

#include "healer.hpp"
#include "../common/config.hpp"
#include "../common/logger.hpp"

#include <chrono>
#include <csignal>
#include <cerrno>
#include <cstring>
#include <sstream>
#include <unistd.h>
#include <sys/types.h>

namespace autoheal {

Healer::Healer(EventQueue& events,
               InterventionLog& log,
               const IgnoreList& ignore,
               bool dry_run)
    : events_(events), log_(log), ignore_(ignore), dry_run_(dry_run) {}

Healer::~Healer() { stop(); }

void Healer::start() {
    if (running_.exchange(true)) return;
    thread_ = std::thread([this] { run_(); });
}

void Healer::stop() {
    if (!running_.exchange(false)) return;
    if (thread_.joinable()) thread_.join();
}

bool Healer::process_alive(int pid) {
    if (pid <= 0) return false;
    if (::kill(pid, 0) == 0) return true;
    return errno != ESRCH;   // EPERM also means it's alive (we just can't signal it)
}

void Healer::record_(const Intervention& iv) {
    std::lock_guard<std::mutex> lk(log_.mu);
    log_.entries.push_back(iv);
    while (log_.entries.size() > InterventionLog::kMaxEntries) {
        log_.entries.pop_front();
    }
}

void Healer::run_() {
    LOG_INFO(std::string("Healer: started") + (dry_run_ ? " (DRY RUN)" : ""));
    using namespace std::chrono;
    while (running_) {
        // Drain new events.
        std::deque<AnomalyEvent> batch;
        {
            std::lock_guard<std::mutex> lk(events_.mu);
            batch.swap(events_.pending);
        }
        for (auto& ev : batch) process_event_(ev);

        escalate_();
        std::this_thread::sleep_for(milliseconds(500));
    }
    LOG_INFO("Healer: stopped");
}

void Healer::process_event_(const AnomalyEvent& ev) {
    // Don't queue duplicate work for a PID we're already healing.
    if (in_flight_.count(ev.pid)) return;

    Pending p;
    p.ev               = ev;
    p.stage            = HealStage::OBSERVED;
    p.stage_entered_at = std::chrono::steady_clock::now();
    in_flight_[ev.pid] = p;

    Intervention iv;
    iv.pid         = ev.pid;
    iv.comm        = ev.comm;
    iv.reason      = ev.type;
    iv.stage       = HealStage::OBSERVED;
    iv.signal_sent = 0;
    iv.outcome     = ev.detail;
    iv.at          = p.stage_entered_at;
    record_(iv);
}

void Healer::escalate_() {
    using namespace std::chrono;
    auto now = steady_clock::now();
    std::vector<int> done;

    for (auto& [pid, p] : in_flight_) {
        // Safety re-check at signal time — never act if PID became protected
        // or vanished, etc.
        ProcessSnapshot fake;
        fake.pid = pid;
        fake.ppid = -1;
        fake.comm = p.ev.comm;
        if (ignore_.should_protect(fake)) {
            LOG_WARN("Healer: refusing to act on protected pid=" + std::to_string(pid));
            done.push_back(pid);
            continue;
        }

        if (!process_alive(pid)) {
            Intervention iv;
            iv.pid     = pid;
            iv.comm    = p.ev.comm;
            iv.reason  = p.ev.type;
            iv.stage   = HealStage::RESOLVED;
            iv.outcome = "process gone";
            iv.at      = now;
            record_(iv);
            done.push_back(pid);
            continue;
        }

        auto seconds_in_stage = duration_cast<seconds>(now - p.stage_entered_at).count();

        switch (p.stage) {
            case HealStage::OBSERVED: {
                // Send SIGSTOP immediately.
                int rc = dry_run_ ? 0 : ::kill(pid, SIGSTOP);
                Intervention iv;
                iv.pid         = pid;
                iv.comm        = p.ev.comm;
                iv.reason      = p.ev.type;
                iv.stage       = HealStage::STOPPED;
                iv.signal_sent = SIGSTOP;
                iv.outcome     = (rc == 0) ? "SIGSTOP sent" :
                                 (std::string("SIGSTOP failed: ") + std::strerror(errno));
                iv.at          = now;
                record_(iv);
                LOG_INFO("Healer SIGSTOP pid=" + std::to_string(pid) + " comm=" + p.ev.comm);

                p.stage            = HealStage::STOPPED;
                p.stage_entered_at = now;
                break;
            }
            case HealStage::STOPPED: {
                if (seconds_in_stage < config::kGraceSigstopToTermSec) break;
                // Continue then terminate.
                if (!dry_run_) ::kill(pid, SIGCONT);
                int rc = dry_run_ ? 0 : ::kill(pid, SIGTERM);
                Intervention iv;
                iv.pid         = pid;
                iv.comm        = p.ev.comm;
                iv.reason      = p.ev.type;
                iv.stage       = HealStage::TERMINATED;
                iv.signal_sent = SIGTERM;
                iv.outcome     = (rc == 0) ? "SIGTERM sent" :
                                 (std::string("SIGTERM failed: ") + std::strerror(errno));
                iv.at          = now;
                record_(iv);
                LOG_INFO("Healer SIGTERM pid=" + std::to_string(pid) + " comm=" + p.ev.comm);

                p.stage            = HealStage::TERMINATED;
                p.stage_entered_at = now;
                break;
            }
            case HealStage::TERMINATED: {
                if (seconds_in_stage < config::kGraceSigtermToKillSec) break;
                int rc = dry_run_ ? 0 : ::kill(pid, SIGKILL);
                Intervention iv;
                iv.pid         = pid;
                iv.comm        = p.ev.comm;
                iv.reason      = p.ev.type;
                iv.stage       = HealStage::KILLED;
                iv.signal_sent = SIGKILL;
                iv.outcome     = (rc == 0) ? "SIGKILL sent" :
                                 (std::string("SIGKILL failed: ") + std::strerror(errno));
                iv.at          = now;
                record_(iv);
                LOG_WARN("Healer SIGKILL pid=" + std::to_string(pid) + " comm=" + p.ev.comm);

                p.stage            = HealStage::KILLED;
                p.stage_entered_at = now;
                break;
            }
            case HealStage::KILLED:
            case HealStage::RESOLVED:
                done.push_back(pid);
                break;
        }
    }

    for (int pid : done) in_flight_.erase(pid);
}

}  // namespace autoheal
