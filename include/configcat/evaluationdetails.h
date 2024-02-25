#pragma once

#include <chrono>
#include <functional>
#include <optional>
#include <string>

#include "config.h"

namespace configcat {

class ConfigCatUser;

struct EvaluationDetails {
public:
    EvaluationDetails(const std::string& key = "",
                      const Value& value = {},
                      const std::optional<std::string>& variationId = "",
                      const std::chrono::time_point<std::chrono::system_clock, std::chrono::duration<double>>& fetchTime = {},
                      const ConfigCatUser* user = nullptr,
                      bool isDefaultValue = false,
                      const std::string& error = "",
                      const TargetingRule* matchedTargetingRule = nullptr,
                      const PercentageOption* matchedPercentageOption = nullptr)
        : key(key)
        , value(value)
        , variationId(variationId)
        , fetchTime(fetchTime)
        , user(user)
        , isDefaultValue(isDefaultValue)
        , error(error)
        // Unfortunately, std::optional<T&> is not possible (https://stackoverflow.com/a/26862721/8656352).
        // We could use std::optional<std::reference_wrapper<T>> as a workaround. However, that would take up more space
        // than pointers, so we'd rather resort to pointers, as this is ctor is not meant for public use.
        , matchedTargetingRule(matchedTargetingRule ? std::optional<TargetingRule>(*matchedTargetingRule) : std::nullopt)
        , matchedPercentageOption(matchedPercentageOption ? std::optional<PercentageOption>(*matchedPercentageOption) : std::nullopt)
    {}

    static EvaluationDetails fromError(const std::string& key, const Value& value, const std::string& error, const std::string& variationId = {}) {
        return EvaluationDetails(key, value, variationId, {}, nullptr, true, error);
    }

    std::string key;
    Value value;
    std::optional<std::string> variationId;
    std::chrono::time_point<std::chrono::system_clock, std::chrono::duration<double>> fetchTime;
    const ConfigCatUser* user;
    bool isDefaultValue;
    std::string error;
    std::optional<TargetingRule> matchedTargetingRule;
    std::optional<PercentageOption> matchedPercentageOption;
};

} // namespace configcat
