#pragma once

#include "log.h"
#include "configcatoptions.h"
#include "configcatuser.h"
#include "config.h"

namespace configcat {

class ConfigCatLogger {
public:
    ConfigCatLogger(const std::shared_ptr<ILogger>& logger, const std::shared_ptr<Hooks>& hooks):
        logger(logger),
        hooks(hooks) {
    }

    void log(LogLevel level, int eventId, const std::string& message) {
        if (hooks && level == LOG_LEVEL_ERROR) {
            hooks->invokeOnError(message);
        }

        if (logger) {
            logger->log(level, "[" + std::to_string(eventId) + "] " + message);
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
    LogEntry(const std::shared_ptr<ConfigCatLogger>& logger, LogLevel level, int eventId) : logger(logger), level(level), eventId(eventId) {}
    ~LogEntry() {
        if (logger && level <= logger->getLogLevel())
            logger->log(level, eventId, message);
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

    // TODO: remove?
    LogEntry& operator<<(const std::shared_ptr<ConfigCatUser>& user) {
        return operator<<(*user);
    }

    // TODO: remove?
    LogEntry& operator<<(const ConfigCatUser& user) {
        if (logger && level <= logger->getLogLevel())
            message += user.toJson();
        return *this;
    }

    // TODO: remove?
    LogEntry& operator<<(const std::optional<Value>& v) {
        if (logger && level <= logger->getLogLevel())
            message += v ? v->toString() : "<invalid value>";
        return *this;
    }

    // TODO: remove?
    LogEntry& operator<<(const SettingValue& v) {
        return *this << static_cast<std::optional<Value>>(v);
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
    int eventId;
    std::string message;
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
