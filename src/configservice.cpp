#include "configservice.h"
#include "configcat/configcatoptions.h"
#include "configcat/timeutils.h"
#include "configcatlogger.h"
#include "configfetcher.h"
#include <iostream>

using namespace std;
using namespace std::this_thread;

namespace configcat {

ConfigService::ConfigService(const string& sdkKey,
                             const shared_ptr<ConfigCatLogger>& logger,
                             const std::shared_ptr<Hooks>& hooks,
                             const std::shared_ptr<ConfigCache>& configCache,
                             const ConfigCatOptions& options):
    logger(logger),
    hooks(hooks),
    pollingMode(options.pollingMode ? options.pollingMode : PollingMode::autoPoll()),
    cachedEntry(const_pointer_cast<ConfigEntry>(ConfigEntry::empty)),
    configCache(configCache) {
    cacheKey = generateCacheKey(sdkKey);
    configFetcher = make_unique<ConfigFetcher>(sdkKey, logger, pollingMode->getPollingIdentifier(), options);
    offline = options.offline;
    startTime = chrono::steady_clock::now();

    if (pollingMode->getPollingIdentifier() == AutoPollingMode::kIdentifier && !offline) {
        startPoll();
    } else {
        setInitialized();
    }
}

ConfigService::~ConfigService() {
    {
        lock_guard<mutex> lock(initMutex);
        stopRequested = true;
    }
    stop.notify_all();
    configFetcher->close();
    if (thread && thread->joinable())
        thread->join();
}

SettingResult ConfigService::getSettings() {
    auto threshold = kDistantPast;
    auto preferCached = initialized;
    if (pollingMode->getPollingIdentifier() == LazyLoadingMode::kIdentifier) {
        auto& lazyPollingMode = (LazyLoadingMode&)*pollingMode;
        threshold = get_utcnowseconds_since_epoch() - lazyPollingMode.cacheRefreshIntervalInSeconds;
        preferCached = false;
    } else if (pollingMode->getPollingIdentifier() == AutoPollingMode::kIdentifier && !initialized) {
        auto& autoPollingMode = (AutoPollingMode&)*pollingMode;
        auto elapsedTime = chrono::duration<double>(chrono::steady_clock::now() - startTime).count();
        threshold = get_utcnowseconds_since_epoch() - autoPollingMode.autoPollIntervalInSeconds;
        if (elapsedTime < autoPollingMode.maxInitWaitTimeInSeconds) {
            unique_lock<mutex> lock(initMutex);
            chrono::duration<double> timeout(autoPollingMode.maxInitWaitTimeInSeconds - elapsedTime);
            init.wait_until(lock, chrono::system_clock::now() + timeout, [&]{ return initialized; });

            // Max wait time expired without result, notify subscribers with the cached config.
            if (!initialized) {
                setInitialized();
                auto config = cachedEntry->config;
                return { (cachedEntry != ConfigEntry::empty && config) ? config->getSettingsOrEmpty() : nullptr, cachedEntry->fetchTime };
            }
        }
    }

    // If we are initialized, we prefer the cached results
    auto [ entry, _0, _1 ] = fetchIfOlder(threshold, preferCached);
    auto config = entry->config;
    return { (cachedEntry != ConfigEntry::empty && config) ? config->getSettingsOrEmpty() : nullptr, entry->fetchTime };
}

RefreshResult ConfigService::refresh() {
    if (offline) {
        auto offlineWarning = "Client is in offline mode, it cannot initiate HTTP calls.";
        LOG_WARN(3200) << offlineWarning;
        return { offlineWarning, nullptr };
    }

    auto [ _, errorMessage, errorException ] = fetchIfOlder(kDistantFuture);
    return { errorMessage, errorException };
}

void ConfigService::setOnline() {
    if (!offline) {
        return;
    }

    offline = false;
    if (pollingMode->getPollingIdentifier() == AutoPollingMode::kIdentifier) {
        startPoll();
    }

    LOG_INFO(5200) << "Switched to ONLINE mode.";
}

void ConfigService::setOffline() {
    if (offline) {
        return;
    }

    offline = true;
    if (pollingMode->getPollingIdentifier() == AutoPollingMode::kIdentifier) {
        {
            lock_guard<mutex> lock(initMutex);
            stopRequested = true;
        }
        stop.notify_all();
        if (thread && thread->joinable())
            thread->join();
    }

    LOG_INFO(5200) << "Switched to OFFLINE mode.";
}

string ConfigService::generateCacheKey(const string& sdkKey) {
    return sha1(sdkKey + "_" + ConfigFetcher::kConfigJsonName + "_" + ConfigEntry::kSerializationFormatVersion);
}
tuple<shared_ptr<const ConfigEntry>, std::optional<std::string>, std::exception_ptr> ConfigService::fetchIfOlder(double threshold, bool preferCached) {
    {
        lock_guard<mutex> lock(fetchMutex);

        // Sync up with the cache and use it when it's not expired.
        auto fromCache = readCache();
        if (fromCache != ConfigEntry::empty && fromCache->eTag != cachedEntry->eTag) {
            cachedEntry = const_pointer_cast<ConfigEntry>(fromCache);
            hooks->invokeOnConfigChanged(fromCache->config->getSettingsOrEmpty());
        }

        // Cache isn't expired
        if (cachedEntry && cachedEntry->fetchTime > threshold) {
            setInitialized();
            return { cachedEntry, nullopt, nullptr };
        }

        // If we are in offline mode or the caller prefers cached values, do not initiate fetch.
        if (offline || preferCached) {
            return { cachedEntry, nullopt, nullptr };
        }

        // If there's an ongoing fetch running, we will wait for the ongoing fetch future and use its response.
        if (!ongoingFetch) {
            // No fetch is running, initiate a new one.
            ongoingFetch = true;
            // launch::deferred will invoke the function on the first calling thread
            responseFuture = async(std::launch::deferred, [&]() {
                auto response = configFetcher->fetchConfiguration(cachedEntry->eTag);
                ongoingFetch = false;
                return response;
            });
        }
    }

    auto response = responseFuture.get();

    lock_guard<mutex> lock(fetchMutex);

    if (response.isFetched()) {
        cachedEntry = const_pointer_cast<ConfigEntry>(response.entry);
        writeCache(cachedEntry);
        hooks->invokeOnConfigChanged(cachedEntry->config->getSettingsOrEmpty());
    } else if ((response.notModified() || !response.isTransientError) && cachedEntry != ConfigEntry::empty) {
        cachedEntry->fetchTime = get_utcnowseconds_since_epoch();
        writeCache(cachedEntry);
    }

    setInitialized();
    return { cachedEntry, response.errorMessage, response.errorException };
}

void ConfigService::setInitialized() {
    if (!initialized) {
        initialized = true;
        init.notify_all();
        hooks->invokeOnClientReady();
    }
}

shared_ptr<const ConfigEntry> ConfigService::readCache() {
    try {
        auto jsonString = configCache->read(cacheKey);
        if (jsonString.empty() || jsonString == cachedEntryString) {
            return ConfigEntry::empty;
        }

        cachedEntryString = jsonString;
        return ConfigEntry::fromString(jsonString);
    } catch (...) {
        LogEntry logEntry(logger, configcat::LOG_LEVEL_ERROR, 2200, current_exception());
        logEntry << "Error occurred while reading the cache.";
        return ConfigEntry::empty;
    }
}

void ConfigService::writeCache(const std::shared_ptr<const ConfigEntry>& configEntry) {
    try {
        configCache->write(cacheKey, configEntry->serialize());
    } catch (...) {
        LogEntry logEntry(logger, configcat::LOG_LEVEL_ERROR, 2201, current_exception());
        logEntry << "Error occurred while writing the cache.";
    }
}

void ConfigService::startPoll() {
    thread = make_unique<std::thread>([this] { run(); });
};

void ConfigService::run() {
    auto& autoPollingMode = (AutoPollingMode&)*pollingMode;
    fetchIfOlder(get_utcnowseconds_since_epoch() - autoPollingMode.autoPollIntervalInSeconds);

    {
        // Initialization finished
        lock_guard<mutex> lock(initMutex);
        setInitialized();
    }
    init.notify_all();

    do {
        {
            unique_lock<mutex> lock(initMutex);
            auto timeout = chrono::seconds(autoPollingMode.autoPollIntervalInSeconds);
            stop.wait_until(lock, chrono::system_clock::now() + timeout, [&]{ return stopRequested; });
            if (stopRequested) {
                break;
            }
        }

        fetchIfOlder(get_utcnowseconds_since_epoch() - autoPollingMode.autoPollIntervalInSeconds);
    } while (true);
}

} // namespace configcat
