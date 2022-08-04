#include "configcat/mapoverridedatasource.h"

using namespace std;

namespace configcat {

MapOverrideDataSource::MapOverrideDataSource(const std::unordered_map<std::string, Value>& source):
    overrides(make_shared<std::unordered_map<std::string, Setting>>()) {
    for (const auto& it : source) {
        overrides->insert({it.first, {it.second}});
    }
}

const std::shared_ptr<std::unordered_map<std::string, Setting>> MapOverrideDataSource::getOverrides() {
    return overrides;
}

} // namespace configcat
