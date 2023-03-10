#pragma once

#include "log.h"
#include "configcatoptions.h"
#include "configcatuser.h"
#include "config.h"

namespace configcat {

//class ConfigCatLogger : public ILogger {
class ConfigCatLogger {
public:
    ConfigCatLogger(std::shared_ptr<ILogger> logger, std::shared_ptr<Hooks> hooks):
        logger(logger),
        hooks(hooks) {
    }

    void log(LogLevel level, const std::string& message) {
        if (hooks && level == LOG_LEVEL_ERROR) {
            hooks->invokeOnError(message);
        }

        if (logger) {
            logger->log(level, message);
        }
    }

    void setLogLevel(LogLevel logLevel) { if (logger) logger->setLogLevel(logLevel); }
    LogLevel getLogLevel() const { return logger ? logger->getLogLevel() : LOG_LEVEL_WARNING; }

private:
    std::shared_ptr<ILogger> logger;
    std::shared_ptr<Hooks> hooks;
};

class LogEntry {
public:
    LogEntry(std::shared_ptr<ConfigCatLogger> logger, LogLevel level): logger(logger), level(level) {}
    ~LogEntry() {
        if (logger && level <= logger->getLogLevel())
            logger->log(level, message);
    }

    LogEntry& operator<<(const char* str) {
        if (str && logger && level <= logger->getLogLevel())
            message += str;
        return *this;
    }

    LogEntry& operator<<(char* str) {
        if (str && logger && level <= logger->getLogLevel())
            message += str;
        return *this;
    }

    LogEntry& operator<<(const std::string& str) {
        if (logger && level <= logger->getLogLevel())
            message += str;
        return *this;
    }

    LogEntry& operator<<(bool arg) {
        if (logger && level <= logger->getLogLevel())
            message += arg ? "true" : "false";
        return *this;
    }

    LogEntry& operator<<(const ConfigCatUser* user) {
        return operator<<(*user);
    }

    LogEntry& operator<<(const ConfigCatUser& user) {
        if (logger && level <= logger->getLogLevel())
            message += user.toJson();
        return *this;
    }

    LogEntry& operator<<(const Value& v) {
        if (logger && level <= logger->getLogLevel())
            message += valueToString(v);
        return *this;
    }

    template<typename Type>
    LogEntry& operator<<(Type arg) {
        if (logger && level <= logger->getLogLevel())
            message += std::to_string(arg);
        return *this;
    }

    template<typename Type>
    LogEntry& operator<<(const std::vector<Type>& v) {
        if (logger && level <= logger->getLogLevel()) {
            message += "[";
            size_t last = v.size() - 1;
            for (size_t i = 0; i < v.size(); ++i) {
                operator<<(v[i]);
                if (i != last) {
                    message += ", ";
                }
            }
            message += "]";
        }
        return *this;
    }

    const std::string& getMessage() { return message; }

private:
    std::shared_ptr<ConfigCatLogger> logger;
    LogLevel level;
    std::string message;
};

} // namespace configcat

// Log macros (requires a shared_ptr<ConfigCatLogger> logger object in the current scope)
#define LOG_ERROR configcat::LogEntry(logger, configcat::LOG_LEVEL_ERROR)
#define LOG_WARN configcat::LogEntry(logger, configcat::LOG_LEVEL_WARNING)
#define LOG_INFO configcat::LogEntry(logger, configcat::LOG_LEVEL_INFO)
#define LOG_DEBUG configcat::LogEntry(logger, configcat::LOG_LEVEL_DEBUG)

#define LOG_ERROR_OBJECT(logger) configcat::LogEntry(logger, configcat::LOG_LEVEL_ERROR)
#define LOG_WARN_OBJECT(logger) configcat::LogEntry(logger, configcat::LOG_LEVEL_WARNING)
#define LOG_INFO_OBJECT(logger) configcat::LogEntry(logger, configcat::LOG_LEVEL_INFO)
#define LOG_DEBUG_OBJECT(logger) configcat::LogEntry(logger, configcat::LOG_LEVEL_DEBUG)
