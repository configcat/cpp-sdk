#include "configcat/log.h"
#include "configcat/configcatuser.h"

namespace configcat {

LogLevel maxLogLevel = LOG_LEVEL_WARNING;
ILogger* logger = nullptr;

void setLogLevel(LogLevel level) { configcat::maxLogLevel = level; }
void setLogger(ILogger* externalLogger) { configcat::logger = externalLogger; }

LogEntry& LogEntry::operator<<(const ConfigCatUser* user) {
    return operator<<(*user);
}

LogEntry& LogEntry::operator<<(const ConfigCatUser& user) {
    if (configcat::logger && level <= configcat::maxLogLevel)
        message += user.toJson();
    return *this;
}

} // namespace configcat
