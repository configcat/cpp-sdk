#pragma once

#include <string>

namespace configcat {

struct ProxyAuthentication {
    std::string user;
    std::string password;
};

} // namespace configcat
