#pragma once

#include <string>

namespace configcat {

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
    virtual void log(LogLevel level, const std::string& message) = 0;
};

extern LogLevel maxLogLevel;
void setLogLevel(LogLevel level);

extern ILogger* logger;
void setLogger(ILogger* externalLogger);

class LogEntry {
public:
    LogEntry(LogLevel level): level(level) {}
    ~LogEntry() {
        if (configcat::logger && level <= configcat::maxLogLevel)
            logger->log(level, message);
    }

    LogEntry& operator<<(const char* str) {
        if (str && configcat::logger && level <= configcat::maxLogLevel)
            message += str;
        return *this;
    }

    LogEntry& operator<<(char* str) {
        if (str && configcat::logger && level <= configcat::maxLogLevel)
            message += str;
        return *this;
    }

    LogEntry& operator<<(const std::string& str) {
        if (configcat::logger && level <= configcat::maxLogLevel)
            message += str;
        return *this;
    }

    template<typename Type>
    LogEntry& operator<<(Type arg) {
        if (configcat::logger && level <= configcat::maxLogLevel)
            message += std::to_string(arg);
        return *this;
    }

private:
    LogLevel level;
    std::string message;
};

} // namespace configcat

// Log macros
#define LOG_ERROR configcat::LogEntry(configcat::LOG_LEVEL_ERROR)
#define LOG_WARN configcat::LogEntry(configcat::LOG_LEVEL_WARNING)
#define LOG_INFO configcat::LogEntry(configcat::LOG_LEVEL_INFO)
#define LOG_DEBUG configcat::LogEntry(configcat::LOG_LEVEL_DEBUG)
