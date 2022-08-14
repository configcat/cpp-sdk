#pragma once

#include <string>
#include <memory>
#include <atomic>

namespace cpr { class Session; }

namespace configcat {

struct ConfigCatOptions;
struct ConfigEntry;
class SessionInterceptor;

enum Status {
    fetched,
    notModified,
    failure
};

struct FetchResponse {
    Status status;
    std::shared_ptr<ConfigEntry> entry;

    bool isFetched() {
        return status == Status::fetched;
    }

    bool notModified() {
        return status == Status::notModified;
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

    ConfigFetcher(const std::string& sdkKey, const std::string& mode, const ConfigCatOptions& options);
    ~ConfigFetcher();

    void close();

    // Fetches the current ConfigCat configuration json.
    FetchResponse fetchConfiguration(const std::string& eTag = "");

private:
    FetchResponse executeFetch(const std::string& eTag, int executeCount);
    FetchResponse fetch(const std::string& eTag);

    std::string sdkKey;
    std::string mode;
    std::shared_ptr<SessionInterceptor> sessionInterceptor;
    std::unique_ptr<cpr::Session> session;
    bool urlIsCustom = false;
    std::string url;
    std::atomic<bool> closed = false;
};

} // namespace configcat
