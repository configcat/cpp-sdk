#include "configcat/refreshpolicy.h"
#include "configcat/configfetcher.h"
#include "configcat/configjsoncache.h"

namespace configcat {

void DefaultRefreshPolicy::refresh() {
    auto response = fetcher.fetchConfiguration();
    if (response.isFetched()) {
        jsonCache.writeCache(response.config);
    }
}

} // namespace configcat
