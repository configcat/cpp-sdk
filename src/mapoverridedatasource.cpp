#include "configcat/mapoverridedatasource.h"

using namespace std;

namespace configcat {

MapFlagOverrides::MapFlagOverrides(const std::unordered_map<std::string, Value>& source, OverrideBehaviour behaviour):
    overrides(make_shared<Settings>()),
    behaviour(behaviour) {
    for (const auto& it : source) {
        overrides->insert({it.first, Setting::fromValue(it.second)});
    }
}

std::shared_ptr<OverrideDataSource> MapFlagOverrides::createDataSource(const std::shared_ptr<ConfigCatLogger>& logger) {
    return make_shared<MapOverrideDataSource>(overrides, behaviour);
}

} // namespace configcat
