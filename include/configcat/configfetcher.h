#pragma once

#include <string>

namespace configcat {

struct ConfigCatOptions;
class ConfigJsonCache;

class ConfigFetcher {
public:
    ConfigFetcher(const std::string& sdkKey, const std::string& mode, ConfigJsonCache& jsonCache, const ConfigCatOptions& options);

    long code = 0;
    std::string data = "";

private:
};

} // namespace configcat
