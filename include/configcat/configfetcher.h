#pragma once

#include <string>
#include <memory>
#include "config.h"

namespace cpr { class Session; }

namespace configcat {

struct ConfigCatOptions;
class ConfigJsonCache;

enum Status {
    fetched,
    notModified,
    failure
};

struct FetchResponse {
    Status status;
    std::shared_ptr<Config> config;

    bool isFetched() {
        return status == fetched;
    }
};

class ConfigFetcher {
public:
    static constexpr char kConfigJsonName[] = "config_v5.json";
    static constexpr char kGlobalBaseUrl[] = "https://cdn-global.configcat.com";
    static constexpr char kEuOnlyBaseUrl[] = "https://cdn-eu.configcat.com";
    static constexpr char kUserAgentHeaderName[] = "X-ConfigCat-UserAgent";
    static constexpr char kIfNoneMatchHeaderName[] = "If-None-Match";
    static constexpr char kEtagHeaderName[] = "Etag";

    ConfigFetcher(const std::string& sdkKey, const std::string& mode, ConfigJsonCache& jsonCache, const ConfigCatOptions& options);
    ~ConfigFetcher();

    // Fetches the current ConfigCat configuration json.
    FetchResponse fetchConfiguration();

    long code = 0;
    std::string data = "";

private:
    FetchResponse executeFetch(int executeCount);

    std::string sdkKey;
    std::string mode;
    ConfigJsonCache& jsonCache;
    std::unique_ptr<cpr::Session> session;
    std::string url;
};

} // namespace configcat
