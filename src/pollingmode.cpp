#include "configcat/pollingmode.h"
#include <memory>

#include "configcat/manualpollingpolicy.h"

using namespace std;

namespace configcat {

shared_ptr<PollingMode> PollingMode::autoPoll(double autoPollIntervalInSeconds,
                                                   uint32_t maxInitWaitTimeInSeconds,
                                                   function<void()> onConfigChanged) {
    return shared_ptr<AutoPollingMode>(
        new AutoPollingMode(autoPollIntervalInSeconds, maxInitWaitTimeInSeconds, onConfigChanged)
    );
}

shared_ptr<PollingMode> PollingMode::lazyLoad(double cacheRefreshIntervalInSeconds) {
    return shared_ptr<LazyLoadingMode>(new LazyLoadingMode(cacheRefreshIntervalInSeconds));
}

shared_ptr<PollingMode> PollingMode::manualPoll() {
    return shared_ptr<ManualPollingMode>(new ManualPollingMode());
}


unique_ptr<RefreshPolicy> AutoPollingMode::createRefreshPolicy(ConfigFetcher& fetcher, ConfigJsonCache& jsonCache) const {
    return {};
}

unique_ptr<RefreshPolicy> LazyLoadingMode::createRefreshPolicy(ConfigFetcher& fetcher, ConfigJsonCache& jsonCache) const {
    return {};
}

unique_ptr<RefreshPolicy> ManualPollingMode::createRefreshPolicy(ConfigFetcher& fetcher, ConfigJsonCache& jsonCache) const {
    return unique_ptr<RefreshPolicy>(new ManualPollingPolicy(fetcher, jsonCache));
}


} // namespace configcat
