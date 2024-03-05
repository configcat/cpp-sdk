#pragma once

#include <stdio.h>
#include "log.h"

namespace configcat {

class ConsoleLogger : public ILogger {
public:
    ConsoleLogger(LogLevel logLevel = LOG_LEVEL_WARNING): ILogger(logLevel) {}

    void log(LogLevel level, const std::string& message, const std::exception_ptr& exception = nullptr) override {
        printf("[%s]: %s\n", logLevelAsString(level), message.c_str());
        if (exception) printf("Exception details: %s\n", unwrap_exception_message(exception).c_str());
    }
};

} // namespace configcat
