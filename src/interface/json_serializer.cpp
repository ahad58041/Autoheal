// src/interface/json_serializer.cpp

#include "json_serializer.hpp"
#include "../common/config.hpp"

#include <nlohmann/json.hpp>

#include <chrono>
#include <unistd.h>

using nlohmann::json;

namespace autoheal {

namespace {

// Clamp string lengths defensively even though nlohmann handles escaping.
std::string capped(const std::string& s, size_t n = 128) {
    if (s.size() <= n) return s;
    return s.substr(0, n);
}

uint64_t to_unix_ms(TimePoint tp) {
    // steady_clock is monotonic — not wall time. Approximate by translating
    // through the gap between steady and system clocks.
    using namespace std::chrono;
    auto sys_now    = system_clock::now();
    auto steady_now = steady_clock::now();
    auto delta      = duration_cast<milliseconds>(tp - steady_now);
    auto sys_then   = sys_now + delta;
    return duration_cast<milliseconds>(sys_then.time_since_epoch()).count();
}

}  // namespace

std::string JsonSerializer::build(const SnapshotBuffer& buffer,
                                  const InterventionLog& log) {
    json out;
    out["ts"]      = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::system_clock::now().time_since_epoch()).count();
    out["ws_port"] = config::kWsPort;
    out["host_pid"] = static_cast<int>(getpid());

    // Build processes array. We also recompute CPU% on the fly from the two
    // latest sweeps so the dashboard sees a meaningful number.
    json procs = json::array();
    {
        std::lock_guard<std::mutex> lk(buffer.mu);
        static const long ticks_per_sec = sysconf(_SC_CLK_TCK);
        static const long ncpus         = sysconf(_SC_NPROCESSORS_ONLN);

        const ProcessTable* latest = &buffer.latest;
        const ProcessTable* prev   = nullptr;
        if (buffer.history.size() >= 2) {
            prev = &buffer.history[buffer.history.size() - 2];
        }

        std::unordered_map<int, const ProcessSnapshot*> prev_index;
        if (prev) {
            for (const auto& p : prev->processes) prev_index[p.pid] = &p;
        }
        double dt_sec = 1.0;
        if (prev) {
            dt_sec = std::chrono::duration<double>(latest->sampled_at - prev->sampled_at).count();
            if (dt_sec <= 0.0) dt_sec = 1.0;
        }

        for (const auto& p : latest->processes) {
            double cpu = 0.0;
            auto it = prev_index.find(p.pid);
            if (it != prev_index.end()) {
                uint64_t a = p.utime_ticks + p.stime_ticks;
                uint64_t b = it->second->utime_ticks + it->second->stime_ticks;
                if (a >= b) {
                    double dticks = static_cast<double>(a - b);
                    cpu = (dticks / static_cast<double>(ticks_per_sec)) / dt_sec * 100.0;
                    if (ncpus > 0) cpu /= static_cast<double>(ncpus);
                }
            }
            procs.push_back({
                {"pid",      p.pid},
                {"ppid",     p.ppid},
                {"comm",     capped(p.comm, 64)},
                {"state",    std::string(1, p.state)},
                {"cpu",      cpu},
                {"rss_kb",   p.rss_kb},
                {"vsize_kb", p.vsize_kb},
                {"uid",      p.uid},
            });
        }
    }
    out["processes"] = procs;

    // Build interventions array (newest last).
    json interventions = json::array();
    {
        std::lock_guard<std::mutex> lk(log.mu);
        for (const auto& iv : log.entries) {
            interventions.push_back({
                {"at_ms",    to_unix_ms(iv.at)},
                {"pid",      iv.pid},
                {"comm",     capped(iv.comm, 64)},
                {"reason",   to_string(iv.reason)},
                {"stage",    to_string(iv.stage)},
                {"signal",   iv.signal_sent},
                {"outcome",  capped(iv.outcome, 128)},
            });
        }
    }
    out["interventions"] = interventions;

    return out.dump();
}

}  // namespace autoheal
