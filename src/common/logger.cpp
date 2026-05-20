// src/common/logger.cpp

#include "logger.hpp"

#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace autoheal {

Logger& Logger::instance() {
    static Logger inst;
    return inst;
}

void Logger::init(const std::string& path, bool mirror_stderr) {
    std::lock_guard<std::mutex> lk(mu_);
    path_          = path;
    mirror_stderr_ = mirror_stderr;
    initialized_   = true;
}

void Logger::info(const std::string& msg)  { write("INFO",  msg); }
void Logger::warn(const std::string& msg)  { write("WARN",  msg); }
void Logger::error(const std::string& msg) { write("ERROR", msg); }

void Logger::write(const char* level, const std::string& msg) {
    std::lock_guard<std::mutex> lk(mu_);

    auto now   = std::chrono::system_clock::now();
    auto ttime = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf{};
    localtime_r(&ttime, &tm_buf);

    std::ostringstream line;
    line << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S")
         << " [" << level << "] " << msg << "\n";

    if (initialized_ && !path_.empty()) {
        std::ofstream out(path_, std::ios::app);
        if (out) out << line.str();
    }
    if (mirror_stderr_ || !initialized_) {
        std::cerr << line.str();
    }
}

}  // namespace autoheal
