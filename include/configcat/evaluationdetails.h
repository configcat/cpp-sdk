#pragma once

#include <string>
#include "config.h"

namespace configcat {

class ConfigCatUser;

struct EvaluationDetails {
public:
    EvaluationDetails(const std::string& key = "",
                      const Value& value = {},
                      const std::string& variationId = "",
                      const std::chrono::time_point<std::chrono::system_clock, std::chrono::duration<double>>& fetchTime = {},
                      const ConfigCatUser* user = nullptr,
                      bool isDefaultValue = false,
                      const std::string& error = "",
                      const RolloutRule* matchedEvaluationRule = nullptr,
                      const RolloutPercentageItem* matchedEvaluationPercentageRule = nullptr)
        : key(key)
        , value(value)
        , variationId(variationId)
        , fetchTime(fetchTime)
        , user(user)
        , isDefaultValue(isDefaultValue)
        , error(error)
        , matchedEvaluationRule(matchedEvaluationRule ? std::optional<RolloutRule>{*matchedEvaluationRule} : std::nullopt)
        , matchedEvaluationPercentageRule(matchedEvaluationPercentageRule ? std::optional<RolloutPercentageItem>{*matchedEvaluationPercentageRule} : std::nullopt)
    {}

    static EvaluationDetails fromError(const std::string& key, const Value& value, const std::string& error, const std::string& variationId = {}) {
        return EvaluationDetails(key, value, variationId, {}, nullptr, true, error);
    }

    std::string key;
    Value value;
    std::string variationId;
    std::chrono::time_point<std::chrono::system_clock, std::chrono::duration<double>> fetchTime;
    const ConfigCatUser* user;
    bool isDefaultValue;
    std::string error;
    std::optional<RolloutRule> matchedEvaluationRule;
    std::optional<RolloutPercentageItem> matchedEvaluationPercentageRule;
};

} // namespace configcat
