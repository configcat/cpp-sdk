#pragma once

#include <string>
#include <vector>
#include <variant>

namespace configcat {

class ConfigCatUser;
struct Value;

enum LogLevel {
    LOG_LEVEL_ERROR,
    LOG_LEVEL_WARNING,
    LOG_LEVEL_INFO,
    LOG_LEVEL_DEBUG
};

inline const char* logLevelAsString(LogLevel level) {
    static const char* const names[] = {
        "Error",
        "Warning",
        "Info",
        "Debug"
    };
    return (LOG_LEVEL_ERROR <= level && level <= LOG_LEVEL_DEBUG) ? names[level] : "<unknown>";
}

class ILogger {
public:
    ILogger(LogLevel logLevel = LOG_LEVEL_WARNING): maxLogLevel(logLevel) {}
    void setLogLevel(LogLevel logLevel) { maxLogLevel = logLevel; }
    LogLevel getLogLevel() const { return maxLogLevel; }

    virtual void log(LogLevel level, const std::string& message) = 0;

protected:
    LogLevel maxLogLevel = LOG_LEVEL_WARNING;
};

} // namespace configcat
