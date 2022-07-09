#include "configcat/pollingmode.h"
#include <memory>

namespace configcat {

std::shared_ptr<PollingMode> PollingMode::autoPoll(double autoPollIntervalInSeconds,
                                                   uint32_t maxInitWaitTimeInSeconds,
                                                   std::function<void()> onConfigChanged) {
    return std::shared_ptr<AutoPollingMode>(
        new AutoPollingMode(autoPollIntervalInSeconds, maxInitWaitTimeInSeconds, onConfigChanged)
    );
}

std::shared_ptr<PollingMode> PollingMode::lazyLoad(double cacheRefreshIntervalInSeconds) {
    return std::shared_ptr<LazyLoadingMode>(new LazyLoadingMode(cacheRefreshIntervalInSeconds));
}

std::shared_ptr<PollingMode> PollingMode::manualPoll() {
    return std::shared_ptr<ManualPollingMode>(new ManualPollingMode());
}


} // namespace configcat
