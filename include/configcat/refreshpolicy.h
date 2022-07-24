#pragma once

namespace configcat {

class Config;
class ConfigFetcher;
class ConfigJsonCache;

class RefreshPolicy {
public:
    virtual const Config& getConfiguration() = 0;
    virtual void close() = 0;
    virtual void refresh() = 0;

    virtual ~RefreshPolicy() = default;
};

class DefaultRefreshPolicy : public RefreshPolicy {
public:
    DefaultRefreshPolicy(ConfigFetcher& fetcher, ConfigJsonCache& jsonCache) {}

    void close() override {}
    void refresh() override {}
};


} // namespace configcat
