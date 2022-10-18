#pragma once

#include <functional>
#include <memory>
#include <string>


namespace configcat {

class RefreshPolicy;
class ConfigFetcher;
class ConfigJsonCache;

// The base class of a polling mode configuration.
class PollingMode {
public:

    /**
     * Creates a configured auto polling configuration.
     *
     * [autoPollIntervalInSeconds] sets at least how often this policy should fetch the latest configuration and refresh the cache.
     * [maxInitWaitTimeInSeconds] sets the maximum waiting time between initialization and the first config acquisition in seconds.
     * [onConfigChanged] sets a configuration changed event handler.
     */
    static std::shared_ptr<PollingMode> autoPoll(uint32_t autoPollIntervalInSeconds = 60,
                                                 uint32_t maxInitWaitTimeInSeconds = 5,
                                                 std::function<void()> onConfigChanged = {});

    /**
     * Creates a configured lazy loading polling configuration.
     *
     * [cacheRefreshIntervalInSeconds] sets how long the cache will store its value before fetching the latest from the network again.
     */
    static std::shared_ptr<PollingMode> lazyLoad(uint32_t cacheRefreshIntervalInSeconds = 60);

    // Creates a configured manual polling configuration.
    static std::shared_ptr<PollingMode> manualPoll();

    // Gets the current polling mode's identifier.
    // Used for analytical purposes in HTTP User-Agent headers.
    virtual const char* getPollingIdentifier() const = 0;
    virtual ~PollingMode() = default;
};

// Represents the auto polling mode's configuration.
class AutoPollingMode : public PollingMode {
    friend class PollingMode;

public:
    static constexpr char kIdentifier[] = "a";

    const uint32_t autoPollIntervalInSeconds;
    const uint32_t maxInitWaitTimeInSeconds;
    const std::function<void()> onConfigChanged;

    const char* getPollingIdentifier() const override { return kIdentifier; }

private:
    AutoPollingMode(uint32_t autoPollIntervalInSeconds, uint32_t maxInitWaitTimeInSeconds, std::function<void()> onConfigChanged):
    autoPollIntervalInSeconds(autoPollIntervalInSeconds),
    maxInitWaitTimeInSeconds(maxInitWaitTimeInSeconds),
    onConfigChanged(onConfigChanged) {
    }
};

// Represents lazy loading mode's configuration.
class LazyLoadingMode : public PollingMode {
    friend class PollingMode;

public:
    static constexpr char kIdentifier[] = "l";

    const uint32_t cacheRefreshIntervalInSeconds;
    const char* getPollingIdentifier() const override { return kIdentifier; }

private:
    LazyLoadingMode(uint32_t cacheRefreshIntervalInSeconds):
    cacheRefreshIntervalInSeconds(cacheRefreshIntervalInSeconds) {
    }
};

// Represents the manual polling mode's configuration.
class ManualPollingMode : public PollingMode {
    friend class PollingMode;

public:
    static constexpr char kIdentifier[] = "m";

    const char* getPollingIdentifier() const override { return kIdentifier; }

private:
    ManualPollingMode() {}
};

} // namespace configcat
