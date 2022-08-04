#pragma once

#include <string>
#include <unordered_map>
#include <memory>

namespace configcat {

struct Setting;

// Describes a data source for [FlagOverrides].
class OverrideDataSource {
public:
    virtual ~OverrideDataSource() = default;

    // Gets all the overrides defined in the given source.
    virtual const std::shared_ptr<std::unordered_map<std::string, Setting>> getOverrides() = 0;
};

} // namespace configcat
