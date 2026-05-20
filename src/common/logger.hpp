// src/common/logger.hpp
//
// Tiny thread-safe logger. Writes timestamped lines to ~/autoheal.log
// AND mirrors to stderr when running in --foreground mode.

#pragma once

#include <string>
#include <mutex>

namespace autoheal {

class Logger {
public:
    static Logger& instance();

    void init(const std::string& path, bool mirror_stderr);
    void info(const std::string& msg);
    void warn(const std::string& msg);
    void error(const std::string& msg);

private:
    Logger() = default;
    void write(const char* level, const std::string& msg);

    std::mutex mu_;
    std::string path_;
    bool mirror_stderr_ = true;
    bool initialized_   = false;
};

#define LOG_INFO(msg)  ::autoheal::Logger::instance().info(msg)
#define LOG_WARN(msg)  ::autoheal::Logger::instance().warn(msg)
#define LOG_ERROR(msg) ::autoheal::Logger::instance().error(msg)

}  // namespace autoheal
