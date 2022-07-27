#include "configcat/manualpollingpolicy.h"
#include "configcat/configjsoncache.h"

using namespace std;

namespace configcat {

shared_ptr<Config> ManualPollingPolicy::getConfiguration() {
    return jsonCache.readCache();
}

} // namespace configcat
