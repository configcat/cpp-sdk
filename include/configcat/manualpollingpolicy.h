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

    std::shared_ptr<Config> getConfiguration() override;
};

} // namespace configcat
