#include "configcat/log.h"
#include "configcat/configcatuser.h"
#include "configcat/config.h"

namespace configcat {

LogLevel maxLogLevel = LOG_LEVEL_WARNING;
ILogger* logger = nullptr;

void setLogLevel(LogLevel level) { configcat::maxLogLevel = level; }
LogLevel getLogLevel() { return configcat::maxLogLevel; }
void setLogger(ILogger* externalLogger) { configcat::logger = externalLogger; }
ILogger* getLogger() { return configcat::logger; }

LogEntry& LogEntry::operator<<(const ConfigCatUser* user) {
    return operator<<(*user);
}

LogEntry& LogEntry::operator<<(const ConfigCatUser& user) {
    if (configcat::logger && level <= configcat::maxLogLevel)
        message += user.toJson();
    return *this;
}

LogEntry& LogEntry::operator<<(const Value& v) {
    if (configcat::logger && level <= configcat::maxLogLevel)
        message += valueToString(v);
    return *this;
}

} // namespace configcat
