#pragma once

#include <string>

namespace configcat {

struct RefreshResult {
    bool success = false;
    std::string error;
};

} // namespace configcat
