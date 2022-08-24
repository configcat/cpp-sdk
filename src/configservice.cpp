#include "configservice.h"
#include "configcat/configcatoptions.h"
#include "configfetcher.h"
#include "configentry.h"
#include <hash-library/sha1.h>
#include <iostream>

using namespace std;
using namespace std::this_thread;

namespace configcat {

// test
std::string get_time()
{
    using namespace std::chrono;
    auto timepoint = system_clock::now();
    auto coarse = system_clock::to_time_t(timepoint);
    auto fine = time_point_cast<std::chrono::milliseconds>(timepoint);

    char buffer[sizeof "9999-12-31 23:59:59.999"];
    std::snprintf(buffer + std::strftime(buffer, sizeof buffer - 3,
                                         "%F %T.", std::localtime(&coarse)),
                  4, "%03lu", fine.time_since_epoch().count() % 1000);

    return buffer;
}
// test


ConfigService::ConfigService(const string& sdkKey, const ConfigCatOptions& options):
    pollingMode(options.mode ? options.mode : PollingMode::autoPoll()),
    cachedEntry(ConfigEntry::empty),
    cache(options.cache) {
    cacheKey = SHA1()(string("cpp_") + ConfigFetcher::kConfigJsonName + "_" + sdkKey);
    configFetcher = make_unique<ConfigFetcher>(sdkKey, pollingMode->getPollingIdentifier(), options);

    if (pollingMode->getPollingIdentifier() == AutoPollingMode::kIdentifier) {
        startTime = chrono::steady_clock::now();
        thread = make_unique<std::thread>([this] { run(); });
    } else {
        initialized = true;
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

const shared_ptr<unordered_map<string, Setting>> ConfigService::getSettings() {
    if (pollingMode->getPollingIdentifier() == LazyLoadingMode::kIdentifier) {
        auto& lazyPollingMode = (LazyLoadingMode&)*pollingMode;
        auto now = chrono::steady_clock::now();
        auto config = fetchIfOlder(now - chrono::seconds(lazyPollingMode.cacheRefreshIntervalInSeconds));
        return config ? config->entries : nullptr;
    } else if (pollingMode->getPollingIdentifier() == AutoPollingMode::kIdentifier && !initialized) {
        auto& autoPollingMode = (AutoPollingMode&)*pollingMode;
        auto elapsedTime = chrono::duration<double>(chrono::steady_clock::now() - startTime).count();
        if (elapsedTime < autoPollingMode.maxInitWaitTimeInSeconds) {
            unique_lock<mutex> lock(initMutex);
            chrono::duration<double> timeout(autoPollingMode.maxInitWaitTimeInSeconds - elapsedTime);
            init.wait_for(lock, timeout, [&]{ return initialized; });

            // Max wait time expired without result, notify subscribers with the cached config.
            if (!initialized) {
                initialized = true;
                auto config = cachedEntry->config;
                return config ? config->entries : nullptr;
            }
        }
    }

    auto config = fetchIfOlder(chrono::time_point<chrono::steady_clock>::min(), true);
    return config ? config->entries : nullptr;
}

void ConfigService::refresh() {
    fetchIfOlder(chrono::time_point<chrono::steady_clock>::max());
}

shared_ptr<Config> ConfigService::fetchIfOlder(chrono::time_point<chrono::steady_clock> time, bool preferCache) {
    std::cout << "ConfigService::fetchIfOlder() - time: " << get_time() << " thread_id: " << this_thread::get_id() << endl;

    {
        lock_guard<mutex> lock(fetchMutex);

        // Sync up with the cache and use it when it's not expired.
        if (cachedEntry == ConfigEntry::empty || cachedEntry->fetchTime > time) {
            auto jsonString = readConfigCache();
            if (!jsonString.empty() && jsonString != cachedEntry->jsonString) {
                auto config = Config::fromJson(jsonString);
                if (config && config != Config::empty) {
                    cachedEntry = make_shared<ConfigEntry>(jsonString, config);
                }
            }
            if (cachedEntry && cachedEntry->fetchTime > time) {
                return cachedEntry->config;
            }
        }

        // Use cache anyway (get calls on auto & manual poll must not initiate fetch).
        // The initialized check ensures that we subscribe for the ongoing fetch during the
        // max init wait time window in case of auto poll.
        if (preferCache && initialized) {
            return cachedEntry->config;
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

    std::cout << "auto response = responseFuture.get(); - time: " << get_time() << " thread_id: " << this_thread::get_id() << endl;
    auto response = responseFuture.get();

    lock_guard<mutex> lock(fetchMutex);

    if (response.isFetched()) {
        cachedEntry = response.entry;
        writeConfigCache(cachedEntry->jsonString);
        if (pollingMode->getPollingIdentifier() == AutoPollingMode::kIdentifier) {
            auto& autoPoll = (AutoPollingMode&)*pollingMode;
            if (autoPoll.onConfigChanged) {
                autoPoll.onConfigChanged();
            }
        }
    } else if (response.notModified()) {
        cachedEntry->fetchTime = chrono::steady_clock::now();
    }

    initialized = true;
    return cachedEntry->config;
}

string ConfigService::readConfigCache() {
    return cache ? cache->read(cacheKey) : "";
}

void ConfigService::writeConfigCache(const string& jsonString) {
    if (cache) {
        cache->write(cacheKey, jsonString);
    }
}

void ConfigService::run() {
    fetchIfOlder(chrono::time_point<chrono::steady_clock>::max());

    {
        // Initialization finished
        lock_guard<mutex> lock(initMutex);
        initialized = true;
    }
    init.notify_all();

    auto& autoPollingMode = (AutoPollingMode&)*pollingMode;
    do {
        {
            unique_lock<mutex> lock(initMutex);
            stop.wait_for(lock, chrono::seconds(autoPollingMode.autoPollIntervalInSeconds), [&]{ return stopRequested; });
            if (stopRequested) {
                break;
            }
        }

        fetchIfOlder(chrono::time_point<chrono::steady_clock>::max());
    } while (true);
}

} // namespace configcat
