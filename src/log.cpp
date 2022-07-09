#include "configcat/log.h"

namespace configcat {

LogLevel maxLogLevel = LOG_LEVEL_WARNING;
ILogger* logger = nullptr;

void setLogLevel(LogLevel level) { configcat::maxLogLevel = level; }
void setLogger(ILogger* externalLogger) { configcat::logger = externalLogger; }

} // namespace configcat
