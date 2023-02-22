#pragma once

#include "overridedatasource.h"
#include "config.h"

namespace configcat {

class MapFlagOverrides : public FlagOverrides {
public:
    MapFlagOverrides(const std::unordered_map<std::string, Value>& source, OverrideBehaviour behaviour);
    std::shared_ptr<OverrideDataSource> createDataSource(std::shared_ptr<ConfigCatLogger> logger) override;

private:
    const std::shared_ptr<std::unordered_map<std::string, Setting>> overrides;
    OverrideBehaviour behaviour;
};

class MapOverrideDataSource : public OverrideDataSource {
public:
    MapOverrideDataSource(const std::shared_ptr<std::unordered_map<std::string, Setting>> overrides,
                          OverrideBehaviour behaviour):
        OverrideDataSource(behaviour),
        overrides(overrides) {
    }

    // Gets all the overrides defined in the given source.
    const std::shared_ptr<std::unordered_map<std::string, Setting>> getOverrides() override { return overrides; }

private:
    const std::shared_ptr<std::unordered_map<std::string, Setting>> overrides;
};

} // namespace configcat
