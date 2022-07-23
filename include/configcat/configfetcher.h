#pragma once

#include <string>

namespace configcat {

struct ConfigCatOptions;

class ConfigFetcher {
public:
    ConfigFetcher(const ConfigCatOptions& options);

    long code = 0;
    std::string data = "";

private:
};

} // namespace configcat
