#pragma once

#include <string>
#include <unordered_map>
#include <memory>
#include "flagoverrides.h"

namespace configcat {

struct Setting;

// Describes a data source for [FlagOverrides].
class OverrideDataSource {
public:
    OverrideDataSource(OverrideBehaviour behaviour): behaviour(behaviour) {}
    virtual ~OverrideDataSource() = default;

    OverrideBehaviour getBehaviour() const { return behaviour; }

    // Gets all the overrides defined in the given source.
    virtual const std::shared_ptr<std::unordered_map<std::string, Setting>> getOverrides() = 0;

private:
    OverrideBehaviour behaviour;
};

} // namespace configcat
