#pragma once

#include <exception>
#include <optional>
#include <string>

namespace configcat {

struct RefreshResult {
    inline bool success() { return !errorMessage; };
    std::optional<std::string> errorMessage;
    std::exception_ptr errorException;
};

} // namespace configcat
