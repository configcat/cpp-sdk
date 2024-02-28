#pragma once

#include <chrono>
#include <exception>

#include "config.h"
#include "configcatuser.h"

namespace configcat {

using fetch_time_t = std::chrono::time_point<std::chrono::system_clock, std::chrono::duration<double>>;

struct EvaluationDetailsBase {
    std::string key;
    std::optional<std::string> variationId;
    configcat::fetch_time_t fetchTime;
    std::shared_ptr<ConfigCatUser> user;
    bool isDefaultValue;
    std::optional<std::string> errorMessage;
    std::optional<std::exception> errorException;
    std::optional<TargetingRule> matchedTargetingRule;
    std::optional<PercentageOption> matchedPercentageOption;

    inline std::optional<Value> value() const { return this->getValue(); }

protected:
    EvaluationDetailsBase(const std::string& key = "",
        const std::optional<std::string>& variationId = "",
        const configcat::fetch_time_t& fetchTime = {},
        const std::shared_ptr<ConfigCatUser>& user = nullptr,
        bool isDefaultValue = false,
        const std::optional<std::string>& errorMessage = std::nullopt,
        const std::optional<std::exception>& errorException = std::nullopt,
        const TargetingRule* matchedTargetingRule = nullptr,
        const PercentageOption* matchedPercentageOption = nullptr)
        : key(key)
        , variationId(variationId)
        , fetchTime(fetchTime)
        , user(user)
        , isDefaultValue(isDefaultValue)
        , errorMessage(errorMessage)
        , errorException(errorException)
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
                      const configcat::fetch_time_t& fetchTime = {},
                      const std::shared_ptr<ConfigCatUser>& user = nullptr,
                      bool isDefaultValue = false,
                      const std::optional<std::string>& errorMessage = std::nullopt,
                      const std::optional<std::exception>& errorException = std::nullopt,
                      const TargetingRule* matchedTargetingRule = nullptr,
                      const PercentageOption* matchedPercentageOption = nullptr) :
        EvaluationDetailsBase(key, variationId, fetchTime, user, isDefaultValue, errorMessage, errorException, matchedTargetingRule, matchedPercentageOption),
        value(value) {
    }

    static EvaluationDetails fromError(const std::string& key,
                                       const ValueType& defaultValue,
                                       const std::string& errorMessage,
                                       const std::optional<std::exception>& errorException = std::nullopt) {
        return EvaluationDetails<ValueType>(key, defaultValue, std::nullopt, {}, nullptr, true, errorMessage, errorException);
    }

    ValueType value;

protected:
    std::optional<Value> getValue() const override {
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
        details.user, details.isDefaultValue, details.errorMessage, details.errorException,
        details.matchedTargetingRule ? &*details.matchedTargetingRule : nullptr,
        details.matchedPercentageOption ? &*details.matchedPercentageOption : nullptr);
}

} // namespace configcat
