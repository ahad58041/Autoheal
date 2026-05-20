// src/brain/brain.cpp
//
// Rules engine. Three behavioral signatures:
//   - CPU_SPIN  : sustained CPU >= kCpuSpinPctThreshold over kWindowSamples
//   - MEM_LEAK  : RSS growing >= kMemLeakKbPerSecThreshold sustained
//   - HUNG      : D-state for kHungSamples consecutive ticks
//
// A PID can only be flagged ONCE per cooldown window. After we emit an event,
// we suppress re-flagging until kCooldownSeconds pass — this gives the Healer
// time to act without us spamming events.

#include "brain.hpp"
#include "../common/config.hpp"
#include "../common/logger.hpp"

#include <chrono>
#include <sstream>
#include <unistd.h>

namespace autoheal {

Brain::Brain(SnapshotBuffer& buffer, EventQueue& events, const IgnoreList& ignore)
    : buffer_(buffer), events_(events), ignore_(ignore) {}

Brain::~Brain() { stop(); }

void Brain::start() {
    if (running_.exchange(true)) return;
    thread_ = std::thread([this] { run_(); });
}

void Brain::stop() {
    if (!running_.exchange(false)) return;
    if (thread_.joinable()) thread_.join();
}

void Brain::run_() {
    LOG_INFO("Brain: started");
    using namespace std::chrono;
    while (running_) {
        evaluate_();
        std::this_thread::sleep_for(milliseconds(config::kSampleIntervalMs));
    }
    LOG_INFO("Brain: stopped");
}

void Brain::evaluate_() {
    // Take a snapshot of the history we'll work with so we don't hold the
    // SnapshotBuffer mutex during analysis.
    std::deque<ProcessTable> hist;
    {
        std::lock_guard<std::mutex> lk(buffer_.mu);
        hist = buffer_.history;
    }
    if (hist.size() < 2) return;

    const ProcessTable& latest = hist.back();
    const ProcessTable& prev   = hist[hist.size() - 2];

    // Build PID -> snapshot lookup for the previous sweep.
    std::unordered_map<int, const ProcessSnapshot*> prev_index;
    for (const auto& p : prev.processes) prev_index[p.pid] = &p;

    static const long ticks_per_sec = sysconf(_SC_CLK_TCK);

    auto dt_sec = std::chrono::duration<double>(latest.sampled_at - prev.sampled_at).count();
    if (dt_sec <= 0.0) dt_sec = 1.0;

    auto now = std::chrono::steady_clock::now();

    for (const auto& cur : latest.processes) {
        if (ignore_.should_protect(cur)) continue;

        // Compute instantaneous CPU% vs previous tick.
        double cpu_pct = 0.0;
        auto it = prev_index.find(cur.pid);
        if (it != prev_index.end()) {
            uint64_t prev_total = it->second->utime_ticks + it->second->stime_ticks;
            uint64_t curr_total = cur.utime_ticks + cur.stime_ticks;
            if (curr_total >= prev_total) {
                double dticks = static_cast<double>(curr_total - prev_total);
                cpu_pct = (dticks / static_cast<double>(ticks_per_sec)) / dt_sec * 100.0;
            }
        }

        Track& tr = tracks_[cur.pid];

        // Cooldown: if we acted recently, skip re-evaluating until it expires.
        if (tr.in_cooldown) {
            auto since = std::chrono::duration_cast<std::chrono::seconds>(now - tr.last_flagged_at).count();
            if (since < config::kCooldownSeconds) continue;
            tr.in_cooldown = false;
        }

        // Rule 1: CPU_SPIN
        if (cpu_pct >= config::kCpuSpinPctThreshold) {
            tr.cpu_high_streak++;
            if (tr.cpu_high_streak >= config::kWindowSamples) {
                AnomalyEvent ev;
                ev.pid        = cur.pid;
                ev.comm       = cur.comm;
                ev.type       = AnomalyType::CPU_SPIN;
                ev.first_seen = now;
                ev.flagged_at = now;
                std::ostringstream d;
                d << "CPU " << static_cast<int>(cpu_pct) << "% for "
                  << tr.cpu_high_streak << " samples";
                ev.detail = d.str();

                {
                    std::lock_guard<std::mutex> lk(events_.mu);
                    events_.pending.push_back(std::move(ev));
                }
                LOG_WARN("Brain flag CPU_SPIN pid=" + std::to_string(cur.pid)
                         + " comm=" + cur.comm);
                tr.cpu_high_streak  = 0;
                tr.last_flagged_at  = now;
                tr.in_cooldown      = true;
                continue;
            }
        } else {
            tr.cpu_high_streak = 0;
        }

        // Rule 2: MEM_LEAK — set a baseline first time we see this PID,
        // then check growth rate over time.
        if (tr.baseline_rss_kb == 0) {
            tr.baseline_rss_kb = cur.rss_kb;
            tr.baseline_at     = now;
        } else {
            auto elapsed_sec = std::chrono::duration<double>(now - tr.baseline_at).count();
            if (elapsed_sec >= config::kWindowSamples /* seconds at 1Hz */) {
                int64_t delta = static_cast<int64_t>(cur.rss_kb) - static_cast<int64_t>(tr.baseline_rss_kb);
                double  rate  = delta / elapsed_sec;
                if (rate >= static_cast<double>(config::kMemLeakKbPerSecThreshold)) {
                    AnomalyEvent ev;
                    ev.pid  = cur.pid;
                    ev.comm = cur.comm;
                    ev.type = AnomalyType::MEM_LEAK;
                    ev.first_seen = tr.baseline_at;
                    ev.flagged_at = now;
                    std::ostringstream d;
                    d << "RSS +" << static_cast<int64_t>(rate / 1024) << " MB/sec ("
                      << cur.rss_kb / 1024 << " MB now)";
                    ev.detail = d.str();
                    {
                        std::lock_guard<std::mutex> lk(events_.mu);
                        events_.pending.push_back(std::move(ev));
                    }
                    LOG_WARN("Brain flag MEM_LEAK pid=" + std::to_string(cur.pid)
                             + " comm=" + cur.comm);
                    tr.baseline_rss_kb = 0;
                    tr.last_flagged_at = now;
                    tr.in_cooldown     = true;
                    continue;
                }
                // Reset baseline periodically so a slow-growing legit process
                // doesn't accumulate a false leak verdict forever.
                if (elapsed_sec > config::kWindowSamples * 4) {
                    tr.baseline_rss_kb = cur.rss_kb;
                    tr.baseline_at     = now;
                }
            }
        }

        // Rule 3: HUNG (D-state)
        if (cur.state == 'D') {
            tr.hung_streak++;
            if (tr.hung_streak >= config::kHungSamples) {
                AnomalyEvent ev;
                ev.pid        = cur.pid;
                ev.comm       = cur.comm;
                ev.type       = AnomalyType::HUNG;
                ev.first_seen = now;
                ev.flagged_at = now;
                ev.detail     = "D-state for " + std::to_string(tr.hung_streak) + " samples";
                {
                    std::lock_guard<std::mutex> lk(events_.mu);
                    events_.pending.push_back(std::move(ev));
                }
                LOG_WARN("Brain flag HUNG pid=" + std::to_string(cur.pid)
                         + " comm=" + cur.comm);
                tr.hung_streak     = 0;
                tr.last_flagged_at = now;
                tr.in_cooldown     = true;
            }
        } else {
            tr.hung_streak = 0;
        }
    }

    // Forget tracks for PIDs that no longer exist.
    std::unordered_map<int, char> alive;
    for (const auto& p : latest.processes) alive[p.pid] = 1;
    for (auto it = tracks_.begin(); it != tracks_.end();) {
        if (!alive.count(it->first)) it = tracks_.erase(it);
        else ++it;
    }
}

}  // namespace autoheal
