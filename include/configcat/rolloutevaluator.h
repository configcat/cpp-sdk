#pragma once

#include <string>
#include <tuple>
#include "config.h"

namespace configcat {

class ConfigCatUser;

class RolloutEvaluator {
public:
    static std::tuple<Value, std::string> evaluate(const Setting& setting, const std::string& key, const ConfigCatUser* user);
    inline static std::string formatNoMatchRule(const std::string& comparisonAttribute,
                                                const std::string* userValue,
                                                Comparator comparator,
                                                const std::string& comparisonValue);
};

} // namespace configcat
