#pragma once

#include <atomic>
#include <string>
#include <memory>
#include <thread>
#include <future>
#include <condition_variable>
#include "configcat/config.h"
#include "configfetcher.h"

namespace configcat {

struct ConfigCatOptions;
class ConfigFetcher;
class ConfigCache;
struct ConfigEntry;
class PollingMode;

class ConfigService {
public:
    ConfigService(const std::string& sdkKey, const ConfigCatOptions& options);
    ~ConfigService();

    const std::shared_ptr<std::unordered_map<std::string, Setting>> getSettings();
    void refresh();

private:
    std::shared_ptr<Config> fetchIfOlder(std::chrono::time_point<std::chrono::steady_clock> time, bool preferCache = false);
    std::string readConfigCache();
    void writeConfigCache(const std::string& jsonString);
    void run();

    std::chrono::time_point<std::chrono::steady_clock> startTime;
    std::mutex initMutex;
    std::mutex fetchMutex;
    std::condition_variable init;
    bool initialized = false;
    std::unique_ptr<std::thread> thread;
    std::condition_variable stop;
    bool stopRequested = false;
    std::atomic<bool> ongoingFetch = false;

    std::shared_ptr<PollingMode> pollingMode;
    std::shared_ptr<ConfigEntry> cachedEntry;
    std::shared_ptr<ConfigCache> cache;
    std::string cacheKey;
    std::unique_ptr<ConfigFetcher> configFetcher;
    std::shared_future<FetchResponse> responseFuture;
};

} // namespace configcat
