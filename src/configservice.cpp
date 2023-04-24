#include "configservice.h"
#include "configcat/configcatoptions.h"
#include "configcat/configcatlogger.h"
#include "configfetcher.h"
#include "utils.h"
#include <hash-library/sha1.h>
#include <iostream>

using namespace std;
using namespace std::this_thread;

namespace configcat {

ConfigService::ConfigService(const string& sdkKey,
                             shared_ptr<ConfigCatLogger> logger,
                             std::shared_ptr<Hooks> hooks,
                             std::shared_ptr<ConfigCache> configCache,
                             const ConfigCatOptions& options):
    logger(logger),
    hooks(hooks),
    pollingMode(options.pollingMode ? options.pollingMode : PollingMode::autoPoll()),
    cachedEntry(ConfigEntry::empty),
    configCache(configCache) {
    cacheKey = SHA1()(string("cpp_") + ConfigFetcher::kConfigJsonName + "_" + sdkKey);
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
    if (pollingMode->getPollingIdentifier() == LazyLoadingMode::kIdentifier) {
        auto& lazyPollingMode = (LazyLoadingMode&)*pollingMode;
        auto now = chrono::steady_clock::now();
        auto [ entry, _ ] = fetchIfOlder(getUtcNowSecondsSinceEpoch() - lazyPollingMode.cacheRefreshIntervalInSeconds);
        auto config = cachedEntry->config;
        return { cachedEntry != ConfigEntry::empty && config ? config->entries : nullptr, entry->fetchTime };
    } else if (pollingMode->getPollingIdentifier() == AutoPollingMode::kIdentifier && !initialized) {
        auto& autoPollingMode = (AutoPollingMode&)*pollingMode;
        auto elapsedTime = chrono::duration<double>(chrono::steady_clock::now() - startTime).count();
        if (elapsedTime < autoPollingMode.maxInitWaitTimeInSeconds) {
            unique_lock<mutex> lock(initMutex);
            chrono::duration<double> timeout(autoPollingMode.maxInitWaitTimeInSeconds - elapsedTime);
            init.wait_for(lock, timeout, [&]{ return initialized; });

            // Max wait time expired without result, notify subscribers with the cached config.
            if (!initialized) {
                setInitialized();
                auto config = cachedEntry->config;
                return { (cachedEntry != ConfigEntry::empty && config) ? config->entries : nullptr, cachedEntry->fetchTime };
            }
        }
    }

    auto [ entry, _ ] = fetchIfOlder(kDistantPast, true);
    auto config = entry->config;
    return { (cachedEntry != ConfigEntry::empty && config) ? config->entries : nullptr, entry->fetchTime };
}

RefreshResult ConfigService::refresh() {
    auto [ _, error ] = fetchIfOlder(kDistantFuture);
    return { error.empty(), error };
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

tuple<shared_ptr<ConfigEntry>, string> ConfigService::fetchIfOlder(double time, bool preferCache) {
    {
        lock_guard<mutex> lock(fetchMutex);

        // Sync up with the cache and use it when it's not expired.
        if (cachedEntry == ConfigEntry::empty || cachedEntry->fetchTime > time) {
            auto entry = readCache();
            if (entry != ConfigEntry::empty && entry->eTag != cachedEntry->eTag) {
                cachedEntry = entry;
                hooks->invokeOnConfigChanged(entry->config->entries);
            }

            // Cache isn't expired
            if (cachedEntry && cachedEntry->fetchTime > time) {
                setInitialized();
                return { cachedEntry, "" };
            }
        }

        // Use cache anyway (get calls on auto & manual poll must not initiate fetch).
        // The initialized check ensures that we subscribe for the ongoing fetch during the
        // max init wait time window in case of auto poll.
        if (preferCache && initialized) {
            return { cachedEntry, "" };
        }

        // If we are in offline mode we are not allowed to initiate fetch.
        if (offline) {
            auto offlineWarning = "Client is in offline mode, it cannot initiate HTTP calls.";
            LOG_WARN(3200) << offlineWarning;
            return { cachedEntry, offlineWarning };
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
        cachedEntry = response.entry;
        writeCache(cachedEntry);
        hooks->invokeOnConfigChanged(cachedEntry->config->entries);
    } else if ((response.notModified() || !response.isTransientError) && cachedEntry != ConfigEntry::empty) {
        cachedEntry->fetchTime = getUtcNowSecondsSinceEpoch();
        writeCache(cachedEntry);
    }

    setInitialized();
    return { cachedEntry, "" };
}

void ConfigService::setInitialized() {
    if (!initialized) {
        initialized = true;
        init.notify_all();
        hooks->invokeOnClientReady();
    }
}

shared_ptr<ConfigEntry> ConfigService::readCache() {
    try {
        auto jsonString = configCache->read(cacheKey);
        if (jsonString.empty() || jsonString == cachedEntryString) {
            return ConfigEntry::empty;
        }

        cachedEntryString = jsonString;
        return ConfigEntry::fromJson(jsonString);
    } catch (exception& exception) {
        LOG_ERROR(2200) << "Error occurred while reading the cache. " << exception.what();
        return ConfigEntry::empty;
    }
}

void ConfigService::writeCache(const std::shared_ptr<ConfigEntry>& configEntry) {
    try {
        configCache->write(cacheKey, configEntry->toJson());
    } catch (exception& exception) {
        LOG_ERROR(2201) << "Error occurred while writing the cache. " << exception.what();
    }
}

void ConfigService::startPoll() {
    thread = make_unique<std::thread>([this] { run(); });
};

void ConfigService::run() {
    auto& autoPollingMode = (AutoPollingMode&)*pollingMode;
    fetchIfOlder(getUtcNowSecondsSinceEpoch() - autoPollingMode.autoPollIntervalInSeconds);

    {
        // Initialization finished
        lock_guard<mutex> lock(initMutex);
        setInitialized();
    }
    init.notify_all();

    do {
        {
            unique_lock<mutex> lock(initMutex);
            stop.wait_for(lock, chrono::seconds(autoPollingMode.autoPollIntervalInSeconds), [&]{ return stopRequested; });
            if (stopRequested) {
                break;
            }
        }

        fetchIfOlder(getUtcNowSecondsSinceEpoch() - autoPollingMode.autoPollIntervalInSeconds);
    } while (true);
}

} // namespace configcat
