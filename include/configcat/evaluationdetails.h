#pragma once

#include <chrono>

#include "config.h"

namespace configcat {

class ConfigCatUser;

struct EvaluationDetailsBase {
    std::string key;
    std::optional<std::string> variationId;
    std::chrono::time_point<std::chrono::system_clock, std::chrono::duration<double>> fetchTime;
    const ConfigCatUser* user;
    bool isDefaultValue;
    std::string error;
    std::optional<TargetingRule> matchedTargetingRule;
    std::optional<PercentageOption> matchedPercentageOption;

    inline std::optional<Value> value() const { return this->getValue(); }

protected:
    EvaluationDetailsBase(const std::string& key = "",
        const std::optional<std::string>& variationId = "",
        const std::chrono::time_point<std::chrono::system_clock, std::chrono::duration<double>>& fetchTime = {},
        const ConfigCatUser* user = nullptr,
        bool isDefaultValue = false,
        const std::string& error = "",
        const TargetingRule* matchedTargetingRule = nullptr,
        const PercentageOption* matchedPercentageOption = nullptr)
        : key(key)
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

    virtual std::optional<Value> getValue() const = 0;
};

template <typename ValueType = std::optional<Value>>
struct EvaluationDetails : public EvaluationDetailsBase {
    EvaluationDetails(const std::string& key = "",
                      const ValueType& value = {},
                      const std::optional<std::string>& variationId = "",
                      const std::chrono::time_point<std::chrono::system_clock, std::chrono::duration<double>>& fetchTime = {},
                      const ConfigCatUser* user = nullptr,
                      bool isDefaultValue = false,
                      const std::string& error = "",
                      const TargetingRule* matchedTargetingRule = nullptr,
                      const PercentageOption* matchedPercentageOption = nullptr) :
        EvaluationDetailsBase(key, variationId, fetchTime, user, isDefaultValue, error, matchedTargetingRule, matchedPercentageOption),
        value(value) {
    }

    static EvaluationDetails fromError(const std::string& key, const ValueType& value, const std::string& error, const std::string& variationId = {}) {
        return EvaluationDetails(key, value, variationId, {}, nullptr, true, error);
    }

    ValueType value;

protected:
    std::optional<Value> getValue() const override
    {
        if constexpr (std::is_same_v<ValueType, std::optional<Value>>) {
            return this->value;
        }
        else {
            return Value(this->value);
        }
    }
};

/** Helper function for creating copies of [EvaluationDetailsBase], which is not constructible, thus, not copyable. */
inline EvaluationDetails<> to_concrete(const EvaluationDetailsBase& details) {
    return EvaluationDetails<>(details.key, details.value(), details.variationId, details.fetchTime,
        details.user, details.isDefaultValue, details.error,
        details.matchedTargetingRule ? &*details.matchedTargetingRule : nullptr,
        details.matchedPercentageOption ? &*details.matchedPercentageOption : nullptr);
}

} // namespace configcat

