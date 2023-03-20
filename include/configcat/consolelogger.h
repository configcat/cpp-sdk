#pragma once

#include <stdio.h>
#include "log.h"

namespace configcat {

class ConsoleLogger : public ILogger {
public:
    ConsoleLogger(LogLevel logLevel = LOG_LEVEL_WARNING): ILogger(logLevel) {}

    void log(LogLevel level, const std::string& message) override {
        printf("[%s]: %s\n", logLevelAsString(level), message.c_str());
    }
};

} // namespace configcat
