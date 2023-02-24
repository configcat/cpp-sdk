#pragma once

#include "config.h"

namespace configcat {

struct SettingResult {
    const std::shared_ptr<std::unordered_map<std::string, Setting>> settings;
    double fetchTime;
};

} // namespace configcat
