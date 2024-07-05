#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>
#include <semver/semver.hpp>

#include "configcat/config.h"
#include "configcat/configcatuser.h"
#include "evaluatelogbuilder.h"

namespace configcat {

class ConfigCatUser;
class ConfigCatLogger;

struct EvaluateContext {
    const std::string& key;
    const Setting& setting;
    const std::shared_ptr<ConfigCatUser>& user;
    const std::shared_ptr<Settings>& settings;

    bool isMissingUserObjectLogged;
    bool isMissingUserObjectAttributeLogged;
    std::shared_ptr<EvaluateLogBuilder> logBuilder; // initialized by RolloutEvaluator.evaluate

    EvaluateContext(
        const std::string& key,
        const Setting& setting,
        const std::shared_ptr<ConfigCatUser>& user,
        const std::shared_ptr<Settings>& settings)
        : key(key)
        , setting(setting)
        , user(user)
        , settings(settings)
        , isMissingUserObjectLogged(false)
        , isMissingUserObjectAttributeLogged(false) {}

    static EvaluateContext forPrerequisiteFlag(
        const std::string& key,
        const Setting& setting,
        EvaluateContext& dependentFlagContext) {

        EvaluateContext context(key, setting, dependentFlagContext.user, dependentFlagContext.settings);
        context.visitedFlags = dependentFlagContext.getVisitedFlags(); // crucial to use `getVisitedFlags` here to make sure the list is created!
        context.logBuilder = dependentFlagContext.logBuilder;
        return context;
    }

    inline std::shared_ptr<std::vector<std::string>> getVisitedFlags() {
        return visitedFlags
            ? visitedFlags
            : (visitedFlags = std::make_shared<std::vector<std::string>>());
    }

private:
    std::shared_ptr<std::vector<std::string>> visitedFlags;
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

    EvaluateResult evaluateSetting(EvaluateContext& context) const;
    std::optional<EvaluateResult> evaluateTargetingRules(const std::vector<TargetingRule>& targetingRules, EvaluateContext& context) const;
    std::optional<EvaluateResult> evaluatePercentageOptions(const std::vector<PercentageOption>& percentageOptions, const TargetingRule* matchedTargetingRule, EvaluateContext& context) const;

    template <typename ContainerType, typename ConditionType>
    RolloutEvaluator::SuccessOrError evaluateConditions(const std::vector<ContainerType>& conditions, const std::function<ConditionType (const ContainerType&)>& conditionAccessor,
        const TargetingRule* targetingRule, const std::string& contextSalt, EvaluateContext& context) const;

    RolloutEvaluator::SuccessOrError evaluateUserCondition(const UserCondition& condition, const std::string& contextSalt, EvaluateContext& context) const;
    bool evaluateTextEquals(const std::string& text, const UserConditionComparisonValue& comparisonValue, bool negate) const;
    bool evaluateSensitiveTextEquals(const std::string& text, const UserConditionComparisonValue& comparisonValue, const std::string& configJsonSalt, const std::string& contextSalt, bool negate) const;
    bool evaluateTextIsOneOf(const std::string& text, const UserConditionComparisonValue& comparisonValue, bool negate) const;
    bool evaluateSensitiveTextIsOneOf(const std::string& text, const UserConditionComparisonValue& comparisonValue, const std::string& configJsonSalt, const std::string& contextSalt, bool negate) const;
    bool evaluateTextSliceEqualsAnyOf(const std::string& text, const UserConditionComparisonValue& comparisonValue, bool startsWith, bool negate) const;
    bool evaluateSensitiveTextSliceEqualsAnyOf(const std::string& text, const UserConditionComparisonValue& comparisonValue, const std::string& configJsonSalt, const std::string& contextSalt, bool startsWith, bool negate) const;
    bool evaluateTextContainsAnyOf(const std::string& text, const UserConditionComparisonValue& comparisonValue, bool negate) const;
    bool evaluateSemVerIsOneOf(const semver::version& version, const UserConditionComparisonValue& comparisonValue, bool negate) const;
    bool evaluateSemVerRelation(const semver::version& version, UserComparator comparator, const UserConditionComparisonValue& comparisonValue) const;
    bool evaluateNumberRelation(double number, UserComparator comparator, const UserConditionComparisonValue& comparisonValue) const;
    bool evaluateDateTimeRelation(double number, const UserConditionComparisonValue& comparisonValue, bool before) const;
    bool evaluateArrayContainsAnyOf(const std::vector<std::string>& array, const UserConditionComparisonValue& comparisonValue, bool negate) const;
    bool evaluateSensitiveArrayContainsAnyOf(const std::vector<std::string>& array, const UserConditionComparisonValue& comparisonValue, const std::string& configJsonSalt, const std::string& contextSalt, bool negate) const;

    bool evaluatePrerequisiteFlagCondition(const PrerequisiteFlagCondition& condition, EvaluateContext& context) const;

    RolloutEvaluator::SuccessOrError evaluateSegmentCondition(const SegmentCondition& condition, EvaluateContext& context) const;

    static std::string userAttributeValueToString(const ConfigCatUser::AttributeValue& attributeValue);

    const std::string& getUserAttributeValueAsText(const std::string& attributeName, const ConfigCatUser::AttributeValue& attributeValue,
        const UserCondition& condition, const std::string& key, std::string& text) const;

    std::variant<const semver::version*, std::string> getUserAttributeValueAsSemVer(const std::string& attributeName, const ConfigCatUser::AttributeValue& attributeValue,
        const UserCondition& condition, const std::string& key, semver::version& version) const;

    std::variant<double, std::string> getUserAttributeValueAsNumber(const std::string& attributeName, const ConfigCatUser::AttributeValue& attributeValue,
        const UserCondition& condition, const std::string& key) const;

    std::variant<double, std::string> getUserAttributeValueAsUnixTimeSeconds(const std::string& attributeName, const ConfigCatUser::AttributeValue& attributeValue,
        const UserCondition& condition, const std::string& key) const;

    std::variant<const std::vector<std::string>*, std::string> getUserAttributeValueAsStringArray(const std::string& attributeName, const ConfigCatUser::AttributeValue& attributeValue,
        const UserCondition& condition, const std::string& key, std::vector<std::string>& array) const;

    void logUserObjectIsMissing(const std::string& key) const;
    void logUserObjectAttributeIsMissingPercentage(const std::string& key, const std::string& attributeName) const;
    void logUserObjectAttributeIsMissingCondition(const std::string& condition, const std::string& key, const std::string& attributeName) const;
    void logUserObjectAttributeIsInvalid(const std::string& condition, const std::string& key, const std::string& reason, const std::string& attributeName) const;
    void logUserObjectAttributeIsAutoConverted(const std::string& condition, const std::string& key, const std::string& attributeName, const std::string& attributeValue) const;
    
    std::string handleInvalidUserAttribute(const UserCondition& condition, const std::string& key, const std::string& attributeName, const std::string& reason) const;
};

} // namespace configcat
