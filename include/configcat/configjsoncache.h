#pragma once

#include <string>
#include "config.h"

namespace configcat {

class ConfigCatCache;


class ConfigJsonCache {
public:
    ConfigJsonCache(std::string sdkKey, std::shared_ptr<ConfigCatCache> cache = nullptr);

    std::shared_ptr<Config> readFromJson(const std::string& json, const std::string& eTag);

    std::shared_ptr<Config> readCache();
    void writeCache(std::shared_ptr<Config> config);


private:
    std::string cacheKey;
    std::shared_ptr<ConfigCatCache> cache;
    std::shared_ptr<Config> inMemoryConfig;
};

} // namespace configcat
