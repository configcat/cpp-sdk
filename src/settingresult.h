#pragma once

#include "configcat/config.h"

namespace configcat {

struct SettingResult {
    std::shared_ptr<Settings> settings;
    double fetchTime;
};

} // namespace configcat
