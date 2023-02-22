#include "configcat/pollingmode.h"
#include <memory>

using namespace std;

namespace configcat {

shared_ptr<PollingMode> PollingMode::autoPoll(uint32_t autoPollIntervalInSeconds,
                                              uint32_t maxInitWaitTimeInSeconds) {
    return shared_ptr<AutoPollingMode>(
        new AutoPollingMode(autoPollIntervalInSeconds, maxInitWaitTimeInSeconds)
    );
}

shared_ptr<PollingMode> PollingMode::lazyLoad(uint32_t cacheRefreshIntervalInSeconds) {
    return shared_ptr<LazyLoadingMode>(new LazyLoadingMode(cacheRefreshIntervalInSeconds));
}

shared_ptr<PollingMode> PollingMode::manualPoll() {
    return shared_ptr<ManualPollingMode>(new ManualPollingMode());
}

} // namespace configcat
