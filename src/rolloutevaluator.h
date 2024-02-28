#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>
#include <hash-library/sha1.h>
#include <hash-library/sha256.h>
#include <semver/semver.hpp>

#include "configcat/config.h"
#include "configcat/configcatuser.h"
#include "evaluatelogbuilder.h"

class SHA1;
class SHA256;

namespace configcat {

class ConfigCatUser;
class ConfigCatLogger;

struct EvaluateContext {
    const std::string& key;
    const Setting& setting;
    const std::shared_ptr<ConfigCatUser>& user;

    bool isMissingUserObjectLogged;
    bool isMissingUserObjectAttributeLogged;
    std::unique_ptr<EvaluateLogBuilder> logBuilder;

    EvaluateContext(
        const std::string& key,
        const Setting& setting,
        const std::shared_ptr<ConfigCatUser>& user)
        : key(key)
        , setting(setting)
        , user(user)
        , isMissingUserObjectLogged(false)
        , isMissingUserObjectAttributeLogged(false) {}
};

struct EvaluateResult {
    const SettingValueContainer& selectedValue;
    const TargetingRule* targetingRule;
    const PercentageOption* percentageOption;
};

class RolloutEvaluator {
public:
    RolloutEvaluator(const std::shared_ptr<ConfigCatLogger>& logger);

    // Evaluate the feature flag or setting
    EvaluateResult evaluate(const std::optional<Value>& defaultValue, EvaluateContext& context, std::optional<Value>& returnValue) const;

private:
    using SuccessOrError = std::variant<bool, std::string>;

    std::shared_ptr<ConfigCatLogger> logger;
    std::unique_ptr<SHA1> sha1;
    std::unique_ptr<SHA256> sha256;

    EvaluateResult evaluateSetting(EvaluateContext& context) const;
    std::optional<EvaluateResult> evaluateTargetingRules(const std::vector<TargetingRule>& targetingRules, EvaluateContext& context) const;
    std::optional<EvaluateResult> evaluatePercentageOptions(const std::vector<PercentageOption>& percentageOptions, const TargetingRule* matchedTargetingRule, EvaluateContext& context) const;

    template <typename ConditionType>
    RolloutEvaluator::SuccessOrError evaluateConditions(const std::vector<ConditionType>& conditions, const std::function<const Condition& (const ConditionType&)>& conditionAccessor,
        const TargetingRule* targetingRule, const std::string& contextSalt, EvaluateContext& context) const;

    RolloutEvaluator::SuccessOrError evaluateUserCondition(const UserCondition& condition, const std::string& contextSalt, EvaluateContext& context) const;
    bool evaluateTextIsOneOf(const std::string& text, const UserConditionComparisonValue& comparisonValue, bool negate) const;
    bool evaluateSensitiveTextIsOneOf(const std::string& text, const UserConditionComparisonValue& comparisonValue, const std::string& configJsonSalt, const std::string& contextSalt, bool negate) const;
    bool evaluateTextContainsAnyOf(const std::string& text, const UserConditionComparisonValue& comparisonValue, bool negate) const;
    bool evaluateSemVerIsOneOf(const semver::version& version, const UserConditionComparisonValue& comparisonValue, bool negate) const;
    bool evaluateSemVerRelation(const semver::version& version, UserComparator comparator, const UserConditionComparisonValue& comparisonValue) const;
    bool evaluateNumberRelation(double number, UserComparator comparator, const UserConditionComparisonValue& comparisonValue) const;

    static std::string userAttributeValueToString(const ConfigCatUser::AttributeValue& attributeValue);
    std::string getUserAttributeValueAsText(const std::string& attributeName, const ConfigCatUser::AttributeValue& attributeValue, const UserCondition& condition, const std::string& key) const;
    std::variant<semver::version, std::string> getUserAttributeValueAsSemVer(const std::string& attributeName, const ConfigCatUser::AttributeValue& attributeValue, const UserCondition& condition, const std::string& key) const;
    std::variant<double, std::string> getUserAttributeValueAsNumber(const std::string& attributeName, const ConfigCatUser::AttributeValue& attributeValue, const UserCondition& condition, const std::string& key) const;

    void logUserObjectIsMissing(const std::string& key) const;
    void logUserObjectAttributeIsMissingPercentage(const std::string& key, const std::string& attributeName) const;
    void logUserObjectAttributeIsMissingCondition(const std::string& condition, const std::string& key, const std::string& attributeName) const;
    void logUserObjectAttributeIsInvalid(const std::string& condition, const std::string& key, const std::string& reason, const std::string& attributeName) const;
    void logUserObjectAttributeIsAutoConverted(const std::string& condition, const std::string& key, const std::string& attributeName, const std::string& attributeValue) const;
    
    std::string handleInvalidUserAttribute(const UserCondition& condition, const std::string& key, const std::string& attributeName, const std::string& reason) const;
};

} // namespace configcat
