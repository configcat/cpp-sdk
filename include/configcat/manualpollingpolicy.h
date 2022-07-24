#pragma once

#include "refreshpolicy.h"

namespace configcat {

class ConfigFetcher;
class ConfigJsonCache;

class ManualPollingPolicy : public DefaultRefreshPolicy {
public:
    ManualPollingPolicy(ConfigFetcher& fetcher, ConfigJsonCache& jsonCache):
        DefaultRefreshPolicy(fetcher, jsonCache) {
    }

    const Config& getConfiguration() override {
    }
};

} // namespace configcat
