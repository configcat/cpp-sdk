#pragma once

#include <string>
#include <memory>
#include <atomic>

namespace cpr { class Session; }

namespace configcat {

struct ConfigCatOptions;
class ConfigCatLogger;
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
    std::string error;
    bool isTransientError = false;

    FetchResponse(Status status, std::shared_ptr<ConfigEntry> entry, const std::string& error = "", bool isTransientError = false):
        status(status),
        entry(entry),
        error(error),
        isTransientError(isTransientError) {
    }

    bool isFetched() {
        return status == Status::fetched;
    }

    bool notModified() {
        return status == Status::notModified;
    }

    bool isFailed() {
        return status == Status::failure;
    }
};

class ConfigFetcher {
public:
    static constexpr char kConfigJsonName[] = "config_v5.json";
    static constexpr char kGlobalBaseUrl[] = "https://cdn-global.configcat.com";
    static constexpr char kEuOnlyBaseUrl[] = "https://cdn-eu.configcat.com";
    static constexpr char kUserAgentHeaderName[] = "X-ConfigCat-UserAgent";
    static constexpr char kPlatformHeaderName[] = "X-ConfigCat-Platform";
    static constexpr char kIfNoneMatchHeaderName[] = "If-None-Match";
    static constexpr char kEtagHeaderName[] = "Etag";

    ConfigFetcher(const std::string& sdkKey, std::shared_ptr<ConfigCatLogger> logger, const std::string& mode, const ConfigCatOptions& options);
    ~ConfigFetcher();

    void close();

    // Fetches the current ConfigCat configuration json.
    FetchResponse fetchConfiguration(const std::string& eTag = "");

private:
    FetchResponse executeFetch(const std::string& eTag, int executeCount);
    FetchResponse fetch(const std::string& eTag);

    std::string sdkKey;
    std::shared_ptr<ConfigCatLogger> logger;
    std::string mode;
    uint32_t connectTimeoutMs; // milliseconds (0 means it never times out during transfer)
    uint32_t readTimeoutMs; // milliseconds (0 means it never times out during transfer)
    std::shared_ptr<SessionInterceptor> sessionInterceptor;
    std::unique_ptr<cpr::Session> session;
    bool urlIsCustom = false;
    std::string url;
    std::atomic<bool> closed = false;
};

} // namespace configcat
