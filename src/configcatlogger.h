#pragma once

#include <exception>

#include "configcat/log.h"
#include "configcat/configcatoptions.h"
#include "configcat/configcatuser.h"
#include "configcat/config.h"
#include "utils.h"

namespace configcat {

class ConfigCatLogger {
public:
    ConfigCatLogger(const std::shared_ptr<ILogger>& logger, const std::shared_ptr<Hooks>& hooks):
        logger(logger),
        hooks(hooks) {
    }

    void log(LogLevel level, int eventId, const std::string& message, const std::optional<std::exception>& ex = std::nullopt) {
        if (hooks && level == LOG_LEVEL_ERROR) {
            hooks->invokeOnError(message);
        }

        if (isEnabled(level)) {
            logger->log(level, "[" + std::to_string(eventId) + "] " + message, ex);
        }
    }

    inline bool isEnabled(LogLevel level) { return logger && level <= logger->getLogLevel(); }

private:
    std::shared_ptr<ILogger> logger;
    std::shared_ptr<Hooks> hooks;
};

class LogEntry {
public:
    LogEntry(const std::shared_ptr<ConfigCatLogger>& logger, LogLevel level, int eventId, const std::optional<std::exception>& ex = std::nullopt)
        : logger(logger), level(level), eventId(eventId), ex(ex) {}

    ~LogEntry() {
        if (logger->isEnabled(level))
            logger->log(level, eventId, message, ex);
    }

    LogEntry& operator<<(const char* str) {
        if (str && logger->isEnabled(level))
            message += str;
        return *this;
    }

    LogEntry& operator<<(char* str) {
        if (str && logger->isEnabled(level))
            message += str;
        return *this;
    }

    LogEntry& operator<<(const std::string& str) {
        if (logger->isEnabled(level))
            message += str;
        return *this;
    }

    LogEntry& operator<<(bool arg) {
        if (logger->isEnabled(level))
            message += arg ? "true" : "false";
        return *this;
    }

    LogEntry& operator<<(const std::optional<Value>& v) {
        if (logger->isEnabled(level))
            message += v ? v->toString() : "";
        return *this;
    }

    template<typename Type>
    LogEntry& operator<<(Type arg) {
        if (logger->isEnabled(level))
            message += std::to_string(arg);
        return *this;
    }

    LogEntry& operator<<(const std::vector<std::string>& v) {
        if (logger->isEnabled(level)) {
            message += "[";
            appendStringList(*this, v);
            message += "]";
        }
        return *this;
    }

    const std::string& getMessage() { return message; }

private:
    std::shared_ptr<ConfigCatLogger> logger;
    LogLevel level;
    int eventId;
    std::string message;
    std::optional<std::exception> ex;
};

} // namespace configcat

// Log macros (requires a shared_ptr<ConfigCatLogger> logger object in the current scope)
#define LOG_ERROR(eventId) configcat::LogEntry(logger, configcat::LOG_LEVEL_ERROR, eventId)
#define LOG_WARN(eventId) configcat::LogEntry(logger, configcat::LOG_LEVEL_WARNING, eventId)
#define LOG_INFO(eventId) configcat::LogEntry(logger, configcat::LOG_LEVEL_INFO, eventId)
#define LOG_DEBUG configcat::LogEntry(logger, configcat::LOG_LEVEL_DEBUG, 0)

#define LOG_ERROR_OBJECT(logger, eventId) configcat::LogEntry(logger, configcat::LOG_LEVEL_ERROR, eventId)
#define LOG_WARN_OBJECT(logger, eventId) configcat::LogEntry(logger, configcat::LOG_LEVEL_WARNING, eventId)
#define LOG_INFO_OBJECT(logger, eventId) configcat::LogEntry(logger, configcat::LOG_LEVEL_INFO, eventId)
#define LOG_DEBUG_OBJECT(logger) configcat::LogEntry(logger, configcat::LOG_LEVEL_DEBUG, 0)
