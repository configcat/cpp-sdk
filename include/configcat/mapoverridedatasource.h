#pragma once

#include "overridedatasource.h"
#include "config.h"

namespace configcat {

class MapOverrideDataSource : public OverrideDataSource {
public:
    MapOverrideDataSource(const std::unordered_map<std::string, Value>& source);

    // Gets all the overrides defined in the given source.
    const std::shared_ptr<std::unordered_map<std::string, Setting>> getOverrides() override;

private:
    const std::shared_ptr<std::unordered_map<std::string, Setting>> overrides;
};

} // namespace configcat
