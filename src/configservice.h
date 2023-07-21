#pragma once

#include <atomic>
#include <string>
#include <memory>
#include <thread>
#include <future>
#include <condition_variable>
#include "configcat/config.h"
#include "configcat/refreshresult.h"
#include "configcat/settingresult.h"
#include "configfetcher.h"


namespace configcat {

struct ConfigCatOptions;
class ConfigCatLogger;
class ConfigFetcher;
class ConfigCache;
struct ConfigEntry;
class PollingMode;
class Hooks;

class ConfigService {
public:
    ConfigService(const std::string& sdkKey,
                  std::shared_ptr<ConfigCatLogger> logger,
                  std::shared_ptr<Hooks> hooks,
                  std::shared_ptr<ConfigCache> configCache,
                  const ConfigCatOptions& options);
    ~ConfigService();

    SettingResult getSettings();
    RefreshResult refresh();
    void setOnline();
    void setOffline();
    bool isOffline() { return offline; }

    static std::string generateCacheKey(const std::string& sdkKey);

private:
    // Returns the ConfigEntry object and error message in case of any error.
    std::tuple<std::shared_ptr<ConfigEntry>, std::string> fetchIfOlder(double time, bool preferCache = false);
    void setInitialized();
    std::shared_ptr<ConfigEntry> readCache();
    void writeCache(const std::shared_ptr<ConfigEntry>& configEntry);
    void startPoll();
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

    std::shared_ptr<ConfigCatLogger> logger;
    std::shared_ptr<Hooks> hooks;
    std::shared_ptr<PollingMode> pollingMode;
    std::shared_ptr<ConfigEntry> cachedEntry;
    std::string cachedEntryString;
    std::shared_ptr<ConfigCache> configCache;
    std::string cacheKey;
    std::unique_ptr<ConfigFetcher> configFetcher;
    std::atomic<bool> offline = false;
    std::shared_future<FetchResponse> responseFuture;
};

} // namespace configcat
