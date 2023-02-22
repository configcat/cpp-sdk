#include "configcat/mapoverridedatasource.h"

using namespace std;

namespace configcat {

MapFlagOverrides::MapFlagOverrides(const std::unordered_map<std::string, Value>& source, OverrideBehaviour behaviour):
    overrides(make_shared<std::unordered_map<std::string, Setting>>()),
    behaviour(behaviour) {
    for (const auto& it : source) {
        overrides->insert({it.first, {it.second}});
    }
}

std::shared_ptr<OverrideDataSource> MapFlagOverrides::createDataSource(std::shared_ptr<ConfigCatLogger> logger) {
    return make_shared<MapOverrideDataSource>(overrides, behaviour);
}

} // namespace configcat
