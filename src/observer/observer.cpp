// src/observer/observer.cpp
//
// Direct, lightweight /proc parser. No external deps. Every read is wrapped
// in try/catch — processes can vanish mid-read and that must NEVER crash us.

#include "observer.hpp"
#include "../common/config.hpp"
#include "../common/logger.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstring>
#include <dirent.h>
#include <fstream>
#include <sstream>
#include <string>
#include <sys/types.h>
#include <unistd.h>

namespace autoheal {

namespace {

// Strict bounds-checked atoi. Returns 0 on garbage rather than throwing.
uint64_t safe_u64(const std::string& s) {
    try {
        if (s.empty()) return 0;
        return std::stoull(s);
    } catch (...) {
        return 0;
    }
}

int safe_i(const std::string& s) {
    try {
        if (s.empty()) return -1;
        return std::stoi(s);
    } catch (...) {
        return -1;
    }
}

// True if the string is purely digits — used to filter /proc subdirs.
bool is_all_digits(const char* s) {
    if (!s || !*s) return false;
    for (const char* p = s; *p; ++p) {
        if (!std::isdigit(static_cast<unsigned char>(*p))) return false;
    }
    return true;
}

// Strip executable / control characters from a comm. Prevents log injection
// and XSS on the dashboard if a process is named something funky.
std::string sanitize_comm(const std::string& in) {
    std::string out;
    out.reserve(in.size());
    for (char c : in) {
        if (static_cast<unsigned char>(c) < 0x20 || c == 0x7f) continue;  // control
        if (c == '"' || c == '\\' || c == '<' || c == '>') {
            out.push_back('_');
            continue;
        }
        out.push_back(c);
    }
    if (out.size() > config::kMaxProcessNameLen) {
        out.resize(config::kMaxProcessNameLen);
    }
    return out;
}

// Parse /proc/[pid]/stat — fields are space-separated, but field 2 (comm)
// is wrapped in parens and may contain spaces. We split on the LAST ')'.
bool parse_stat(int pid, ProcessSnapshot& out) {
    std::ifstream f("/proc/" + std::to_string(pid) + "/stat");
    if (!f) return false;

    std::string line;
    if (!std::getline(f, line)) return false;

    auto rparen = line.rfind(')');
    if (rparen == std::string::npos) return false;

    auto lparen = line.find('(');
    if (lparen == std::string::npos || lparen >= rparen) return false;

    out.pid  = pid;
    out.comm = sanitize_comm(line.substr(lparen + 1, rparen - lparen - 1));

    // Fields after ')' — split by whitespace.
    std::istringstream rest(line.substr(rparen + 2));
    std::vector<std::string> fields;
    std::string tok;
    while (rest >> tok) fields.push_back(tok);

    // Reference: man 5 proc — "/proc/[pid]/stat" field numbering starts at 1
    // with `pid`; after we removed pid+comm we have field index 3 onward.
    // We need: state(3), ppid(4), utime(14), stime(15), starttime(22).
    // In the `fields` vector those are indices 0, 1, 11, 12, 19.
    if (fields.size() < 20) return false;

    out.state       = fields[0].empty() ? '?' : fields[0][0];
    out.ppid        = safe_i(fields[1]);
    out.utime_ticks = safe_u64(fields[11]);
    out.stime_ticks = safe_u64(fields[12]);
    out.starttime   = safe_u64(fields[19]);
    return true;
}

// Parse /proc/[pid]/status for VmRSS, VmSize, Uid.
bool parse_status(int pid, ProcessSnapshot& out) {
    std::ifstream f("/proc/" + std::to_string(pid) + "/status");
    if (!f) return false;

    std::string line;
    while (std::getline(f, line)) {
        if (line.compare(0, 6, "VmRSS:") == 0) {
            std::istringstream is(line.substr(6));
            uint64_t kb = 0; is >> kb;
            out.rss_kb = kb;
        } else if (line.compare(0, 7, "VmSize:") == 0) {
            std::istringstream is(line.substr(7));
            uint64_t kb = 0; is >> kb;
            out.vsize_kb = kb;
        } else if (line.compare(0, 4, "Uid:") == 0) {
            std::istringstream is(line.substr(4));
            int uid = -1; is >> uid;
            out.uid = uid;
        }
    }
    return true;
}

}  // namespace

std::optional<ProcessSnapshot> Observer::parse_one(int pid) {
    ProcessSnapshot snap;
    try {
        if (!parse_stat(pid, snap)) return std::nullopt;
        parse_status(pid, snap);   // best effort — kernel threads may not have all fields
        snap.sampled_at = std::chrono::steady_clock::now();
        return snap;
    } catch (...) {
        return std::nullopt;       // process died mid-read — skip silently
    }
}

Observer::Observer(SnapshotBuffer& buffer) : buffer_(buffer) {}

Observer::~Observer() { stop(); }

void Observer::start() {
    if (running_.exchange(true)) return;
    thread_ = std::thread([this] { run_(); });
}

void Observer::stop() {
    if (!running_.exchange(false)) return;
    if (thread_.joinable()) thread_.join();
}

ProcessTable Observer::sweep_() {
    ProcessTable table;
    table.sampled_at = std::chrono::steady_clock::now();

    DIR* dir = opendir("/proc");
    if (!dir) {
        LOG_ERROR("Observer: cannot open /proc");
        return table;
    }

    struct dirent* ent = nullptr;
    while ((ent = readdir(dir)) != nullptr) {
        if (!is_all_digits(ent->d_name)) continue;
        int pid = safe_i(ent->d_name);
        if (pid <= 0) continue;

        auto snap = parse_one(pid);
        if (snap) table.processes.push_back(std::move(*snap));
    }
    closedir(dir);
    return table;
}

void Observer::run_() {
    LOG_INFO("Observer: started");
    using namespace std::chrono;
    while (running_) {
        auto tick_start = steady_clock::now();

        ProcessTable table = sweep_();
        {
            std::lock_guard<std::mutex> lk(buffer_.mu);
            buffer_.latest = table;
            buffer_.history.push_back(std::move(table));
            while (buffer_.history.size() > SnapshotBuffer::kMaxHistory) {
                buffer_.history.pop_front();
            }
        }

        auto elapsed = duration_cast<milliseconds>(steady_clock::now() - tick_start);
        auto remaining = milliseconds(config::kSampleIntervalMs) - elapsed;
        if (remaining.count() > 0) std::this_thread::sleep_for(remaining);
    }
    LOG_INFO("Observer: stopped");
}

}  // namespace autoheal
