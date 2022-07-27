#pragma once

#include <memory>

namespace configcat {

struct Config;
class ConfigFetcher;
class ConfigJsonCache;

class RefreshPolicy {
public:
    virtual std::shared_ptr<Config> getConfiguration() = 0;
    virtual void close() = 0;
    virtual void refresh() = 0;

    virtual ~RefreshPolicy() = default;
};

class DefaultRefreshPolicy : public RefreshPolicy {
public:
    DefaultRefreshPolicy(ConfigFetcher& fetcher, ConfigJsonCache& jsonCache):
        fetcher(fetcher),
        jsonCache(jsonCache) {
    }

    void close() override {}
    void refresh() override;

protected:
    ConfigFetcher& fetcher;
    ConfigJsonCache& jsonCache;
};


} // namespace configcat
