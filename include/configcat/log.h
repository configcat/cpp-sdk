#pragma once

#include <exception>
#include <optional>
#include <string>
#include <vector>
#include <variant>

namespace configcat {

class ConfigCatUser;

enum LogLevel {
    LOG_LEVEL_ERROR,
    LOG_LEVEL_WARNING,
    LOG_LEVEL_INFO,
    LOG_LEVEL_DEBUG
};

inline const char* logLevelAsString(LogLevel level) {
    static const char* const names[] = {
        "ERROR",
        "WARNING",
        "INFO",
        "DEBUG"
    };
    return (LOG_LEVEL_ERROR <= level && level <= LOG_LEVEL_DEBUG) ? names[level] : "<unknown>";
}

class ILogger {
public:
    ILogger(LogLevel logLevel = LOG_LEVEL_WARNING): maxLogLevel(logLevel) {}
    void setLogLevel(LogLevel logLevel) { maxLogLevel = logLevel; }
    LogLevel getLogLevel() const { return maxLogLevel; }

    virtual void log(LogLevel level, const std::string& message, const std::exception_ptr& exception = nullptr) = 0;

protected:
    LogLevel maxLogLevel = LOG_LEVEL_WARNING;
};

} // namespace configcat

static inline std::string unwrap_exception_message(const std::exception_ptr& eptr) {
    // Based on: https://stackoverflow.com/a/37222762/8656352
    if (eptr) {
        try { std::rethrow_exception(eptr); }
        catch (const std::exception& ex) { return ex.what(); }
        catch (const std::string& ex) { return ex; }
        catch (const char* ex) { return ex; }
        catch (...) { return "<unknown error>"; }
    }
    return "<not available>";
}
