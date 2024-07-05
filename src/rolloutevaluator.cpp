#include <assert.h>
#include <exception>
#include <ostream> // must be imported before <semver/semver.hpp>
#include <sstream>

#include <nlohmann/json.hpp>
#include <semver/semver.hpp>

#include "configcatlogger.h"
#include "rolloutevaluator.h"

using namespace std;

namespace configcat {

static constexpr char kTargetingRuleIgnoredMessage[] = "The current targeting rule is ignored and the evaluation continues with the next rule.";
static constexpr char kMissingUserObjectError[] = "cannot evaluate, User Object is missing";
static constexpr char kMissingUserAttributeError[] = "cannot evaluate, the User.%s attribute is missing";
static constexpr char kInvalidUserAttributeError[] = "cannot evaluate, the User.%s attribute is invalid (%s)";

static const string& ensureConfigJsonSalt(const shared_ptr<string>& value) {
    if (value) {
        return *value;
    }
    throw runtime_error("Config JSON salt is missing.");
}

template <typename ValueType>
static const ValueType& ensureComparisonValue(const UserConditionComparisonValue& comparisonValue) {
    if (const auto comparisonValuePtr = get_if<ValueType>(&comparisonValue); comparisonValuePtr) {
        return *comparisonValuePtr;
    }
    throw runtime_error("Comparison value is missing or invalid.");
}

static string hashComparisonValue(const string& value, const string& configJsonSalt, const string& contextSalt) {
    return sha256(value + configJsonSalt + contextSalt);
}

RolloutEvaluator::RolloutEvaluator(const std::shared_ptr<ConfigCatLogger>& logger) :
    logger(logger) {
}

EvaluateResult RolloutEvaluator::evaluate(const std::optional<Value>& defaultValue, EvaluateContext& context, std::optional<Value>& returnValue) const {
    auto& logBuilder = context.logBuilder;

    // Building the evaluation log is expensive, so let's not do it if it wouldn't be logged anyway.
    if (logger->isEnabled(LogLevel::LOG_LEVEL_INFO)) {
        logBuilder = make_shared<EvaluateLogBuilder>();

        logBuilder->appendFormat("Evaluating '%s'", context.key.c_str());

        if (context.user) {
            logBuilder->appendFormat(" for User '%s'", context.user->toJson().c_str());
        }

        logBuilder->increaseIndent();
    }

    const auto log = [](
        const std::optional<Value>& returnValue,
        const shared_ptr<ConfigCatLogger>& logger,
        const shared_ptr<EvaluateLogBuilder>& logBuilder
        ) {
            if (logBuilder) {
                logBuilder->newLine().appendFormat("Returning '%s'.", returnValue ? returnValue->toString().c_str() : "");
                logBuilder->decreaseIndent();

                logger->log(LogLevel::LOG_LEVEL_INFO, 5000, logBuilder->toString());
            }
        };

    try {
        const auto settingType = context.setting.getTypeChecked();

        if (defaultValue) {
            const auto expectedSettingType = static_cast<SettingValue>(*defaultValue).getSettingType();

            if (settingType != expectedSettingType) {
                const char* settingTypeFormatted = getSettingTypeText(settingType);
                const char* defaultValueTypeFormatted = getSettingValueTypeText(static_cast<SettingValue>(*defaultValue));
                throw runtime_error(string_format(
                    "The type of a setting must match the type of the specified default value. "
                    "Setting's type was %s but the default value's type was %s. "
                    "Please use a default value which corresponds to the setting type %s. "
                    "Learn more: https://configcat.com/docs/sdk-reference/cpp/#setting-type-mapping",
                    settingTypeFormatted, defaultValueTypeFormatted, settingTypeFormatted));
            }
        }

        auto result = evaluateSetting(context);
        returnValue = result.selectedValue.value.toValueChecked(settingType);
        // At this point it's ensured that the return value is compatible with the default value
        // (specifically, with the return type of the evaluation method overload that was called).

        log(returnValue, logger, logBuilder);
        return result;
    }
    catch (...) {
        auto ex = current_exception();
        if (logBuilder) logBuilder->resetIndent().increaseIndent();

        log(defaultValue, logger, logBuilder);
        rethrow_exception(ex);
    }
}

EvaluateResult RolloutEvaluator::evaluateSetting(EvaluateContext& context) const {
    const auto& targetingRules = context.setting.targetingRules;
    if (!targetingRules.empty()) {
        auto evaluateResult = evaluateTargetingRules(targetingRules, context);
        if (evaluateResult) {
            return std::move(*evaluateResult);
        }
    }

    const auto& percentageOptions = context.setting.percentageOptions;
    if (!percentageOptions.empty()) {
        auto evaluateResult = evaluatePercentageOptions(percentageOptions, nullptr, context);
        if (evaluateResult) {
            return std::move(*evaluateResult);
        }
    }

    return { context.setting, nullptr, nullptr };
}

std::optional<EvaluateResult> RolloutEvaluator::evaluateTargetingRules(const std::vector<TargetingRule>& targetingRules, EvaluateContext& context) const {
    const auto& logBuilder = context.logBuilder;

    if (logBuilder) logBuilder->newLine("Evaluating targeting rules and applying the first match if any:");

    const auto conditionAccessor = std::function([](const ConditionContainer& container) -> const Condition& { return container.condition; });

    for (const auto& targetingRule : targetingRules) {
        const std::vector<ConditionContainer>& conditions = targetingRule.conditions;

        const auto isMatchOrError = evaluateConditions(conditions, conditionAccessor, &targetingRule, context.key, context);

        if (const auto isMatchPtr = get_if<bool>(&isMatchOrError); !isMatchPtr || !*isMatchPtr) {
            if (!isMatchPtr) {
                if (logBuilder) {
                    logBuilder->increaseIndent()
                        .newLine(kTargetingRuleIgnoredMessage)
                        .decreaseIndent();
                }
            }
            continue;
        }

        if (const auto simpleValuePtr = get_if<SettingValueContainer>(&targetingRule.then); simpleValuePtr) {
            return EvaluateResult{ *simpleValuePtr, &targetingRule, nullptr };
        }

        const auto percentageOptionsPtr = get_if<PercentageOptions>(&targetingRule.then);
        if (!percentageOptionsPtr || percentageOptionsPtr->empty()) {
            throw runtime_error("Targeting rule THEN part is missing or invalid.");
        }

        if (logBuilder) logBuilder->increaseIndent();

        auto evaluateResult = evaluatePercentageOptions(*percentageOptionsPtr, &targetingRule, context);
        if (evaluateResult) {
            if (logBuilder) logBuilder->decreaseIndent();

            return std::move(*evaluateResult);
        }

        if (logBuilder) {
            logBuilder->newLine(kTargetingRuleIgnoredMessage)
                .decreaseIndent();
        }
    }

    return nullopt;
}

std::optional<EvaluateResult> RolloutEvaluator::evaluatePercentageOptions(const std::vector<PercentageOption>& percentageOptions, const TargetingRule* matchedTargetingRule, EvaluateContext& context) const {
    const auto& logBuilder = context.logBuilder;

    if (!context.user) {
        if (logBuilder) logBuilder->newLine("Skipping % options because the User Object is missing.");

        if (!context.isMissingUserObjectLogged) {
            logUserObjectIsMissing(context.key);
            context.isMissingUserObjectLogged = true;
        }

        return nullopt;
    }

    const auto& percentageOptionsAttributeName = context.setting.percentageOptionsAttribute;
    const auto percentageOptionsAttributeValuePtr = percentageOptionsAttributeName
        ? context.user->getAttribute(percentageOptionsAttributeName.value_or(ConfigCatUser::kIdentifierAttribute))
        : &context.user->getIdentifierAttribute();

    if (!percentageOptionsAttributeValuePtr) {
        if (logBuilder) {
            logBuilder->newLine().appendFormat("Skipping %% options because the User.%s attribute is missing.",
                percentageOptionsAttributeName ? percentageOptionsAttributeName->c_str() : ConfigCatUser::kIdentifierAttribute);
        }

        if (!context.isMissingUserObjectAttributeLogged) {
            logUserObjectAttributeIsMissingPercentage(context.key,
                percentageOptionsAttributeName ? percentageOptionsAttributeName->c_str() : ConfigCatUser::kIdentifierAttribute);
            context.isMissingUserObjectAttributeLogged = true;
        }

        return nullopt;
    }

    if (logBuilder) {
        logBuilder->newLine().appendFormat("Evaluating %% options based on the User.%s attribute:",
            percentageOptionsAttributeName ? percentageOptionsAttributeName->c_str() : ConfigCatUser::kIdentifierAttribute);
    }

    const auto userAttributeValuePtr = get_if<string>(percentageOptionsAttributeValuePtr);
    const auto userAttributeValue = userAttributeValuePtr ? string() : userAttributeValueToString(*percentageOptionsAttributeValuePtr);

    auto hash = sha1(context.key + (userAttributeValuePtr ? *userAttributeValuePtr : userAttributeValue));
    const auto hashValue = std::stoul(hash.erase(7), nullptr, 16) % 100;

    if (logBuilder) {
        logBuilder->newLine().appendFormat("- Computing hash in the [0..99] range from User.%s => %d (this value is sticky and consistent across all SDKs)",
            percentageOptionsAttributeName ? percentageOptionsAttributeName->c_str() : ConfigCatUser::kIdentifierAttribute, hashValue);
    }

    auto bucket = 0u;
    auto optionNumber = 1;

    for (const auto& percentageOption : percentageOptions) {
        auto percentage = percentageOption.percentage;
        bucket += percentage;

        if (hashValue >= bucket) {
            ++optionNumber;
            continue;
        }

        if (logBuilder) {
            string str;
            logBuilder->newLine().appendFormat("- Hash value %d selects %% option %d (%d%%), '%s'.",
                hashValue, optionNumber, percentage, formatSettingValue(percentageOption.value, str).c_str());
        }

        return EvaluateResult{ percentageOption, matchedTargetingRule, &percentageOption };
    }

    throw runtime_error("Sum of percentage option percentages is less than 100.");
}

template <typename ContainerType, typename ConditionType>
RolloutEvaluator::SuccessOrError RolloutEvaluator::evaluateConditions(const std::vector<ContainerType>& conditions, const std::function<ConditionType (const ContainerType&)>& conditionAccessor,
    const TargetingRule* targetingRule, const std::string& contextSalt, EvaluateContext& context) const {

    RolloutEvaluator::SuccessOrError result = true;

    const auto& logBuilder = context.logBuilder;
    auto newLineBeforeThen = false;

    if (logBuilder) logBuilder->newLine("- ");

    auto i = 0;
    for (const auto& it : conditions) {
        const ConditionType& condition = conditionAccessor(it);

        if (logBuilder) {
            if (i == 0) {
                logBuilder->append("IF ")
                    .increaseIndent();
            } else {
                logBuilder->increaseIndent()
                    .newLine("AND ");
            }
        }

        if (const auto userConditionPtr = get_if<UserCondition>(&condition); userConditionPtr) {
            result = evaluateUserCondition(*userConditionPtr, contextSalt, context);
            newLineBeforeThen = conditions.size() > 1;
        } else if (const auto prerequisiteFlagConditionPtr = get_if<PrerequisiteFlagCondition>(&condition); prerequisiteFlagConditionPtr) {
            result = evaluatePrerequisiteFlagCondition(*prerequisiteFlagConditionPtr, context);
            newLineBeforeThen = true;
        } else if (const auto segmentConditionPtr = get_if<SegmentCondition>(&condition); segmentConditionPtr) {
            result = evaluateSegmentCondition(*segmentConditionPtr, context);
            newLineBeforeThen = !holds_alternative<string>(result) || get<string>(result) != kMissingUserObjectError || conditions.size() > 1;
        } else {
            throw runtime_error("Condition is missing or invalid.");
        }

        const auto successPtr = get_if<bool>(&result);
        const auto success = successPtr && *successPtr;

        if (logBuilder) {
            if (!targetingRule || conditions.size() > 1) {
                logBuilder->appendConditionConsequence(success);
            }

            logBuilder->decreaseIndent();
        }

        if (!success) break;

        ++i;
    }

    if (targetingRule) {
        if (logBuilder) logBuilder->appendTargetingRuleConsequence(*targetingRule, context.setting.type, result, newLineBeforeThen);
    }

    return result;
}

RolloutEvaluator::SuccessOrError RolloutEvaluator::evaluateUserCondition(const UserCondition& condition, const std::string& contextSalt, EvaluateContext& context) const {
    const auto& logBuilder = context.logBuilder;
    if (logBuilder) logBuilder->appendUserCondition(condition);

    if (!context.user) {
        if (!context.isMissingUserObjectLogged) {
            logUserObjectIsMissing(context.key);
            context.isMissingUserObjectLogged = true;
        }

        return string(kMissingUserObjectError);
    }

    const auto& userAttributeName = condition.comparisonAttribute;
    const auto userAttributeValuePtr = context.user->getAttribute(userAttributeName);

    if (const string* textPtr; !userAttributeValuePtr || (textPtr = get_if<string>(userAttributeValuePtr)) && textPtr->empty()) {
        logUserObjectAttributeIsMissingCondition(formatUserCondition(condition), context.key, userAttributeName);

        return string_format(kMissingUserAttributeError, userAttributeName.c_str());
    }

    const auto comparator = condition.comparator;

    switch (comparator) {
    case UserComparator::TextEquals:
    case UserComparator::TextNotEquals: {
        string text;
        const auto& textRef = getUserAttributeValueAsText(userAttributeName, *userAttributeValuePtr, condition, context.key, text);

        return evaluateTextEquals(
            textRef,
            condition.comparisonValue,
            comparator == UserComparator::TextNotEquals
        );
    }

    case UserComparator::SensitiveTextEquals:
    case UserComparator::SensitiveTextNotEquals: {
        string text;
        const auto& textRef = getUserAttributeValueAsText(userAttributeName, *userAttributeValuePtr, condition, context.key, text);

        return evaluateSensitiveTextEquals(
            textRef,
            condition.comparisonValue,
            ensureConfigJsonSalt(context.setting.configJsonSalt),
            contextSalt,
            comparator == UserComparator::SensitiveTextNotEquals
        );
    }

    case UserComparator::TextIsOneOf:
    case UserComparator::TextIsNotOneOf: {
        string text;
        const auto& textRef = getUserAttributeValueAsText(userAttributeName, *userAttributeValuePtr, condition, context.key, text);

        return evaluateTextIsOneOf(
            textRef,
            condition.comparisonValue,
            comparator == UserComparator::TextIsNotOneOf
        );
    }

    case UserComparator::SensitiveTextIsOneOf:
    case UserComparator::SensitiveTextIsNotOneOf: {
        string text;
        const auto& textRef = getUserAttributeValueAsText(userAttributeName, *userAttributeValuePtr, condition, context.key, text);

        return evaluateSensitiveTextIsOneOf(
            textRef,
            condition.comparisonValue,
            ensureConfigJsonSalt(context.setting.configJsonSalt),
            contextSalt,
            comparator == UserComparator::SensitiveTextIsNotOneOf
        );
    }

    case UserComparator::TextStartsWithAnyOf:
    case UserComparator::TextNotStartsWithAnyOf: {
        string text;
        const auto& textRef = getUserAttributeValueAsText(userAttributeName, *userAttributeValuePtr, condition, context.key, text);

        return evaluateTextSliceEqualsAnyOf(
            textRef,
            condition.comparisonValue,
            true,
            comparator == UserComparator::TextNotStartsWithAnyOf
        );
    }

    case UserComparator::SensitiveTextStartsWithAnyOf:
    case UserComparator::SensitiveTextNotStartsWithAnyOf: {
        string text;
        const auto& textRef = getUserAttributeValueAsText(userAttributeName, *userAttributeValuePtr, condition, context.key, text);

        return evaluateSensitiveTextSliceEqualsAnyOf(
            textRef,
            condition.comparisonValue,
            ensureConfigJsonSalt(context.setting.configJsonSalt),
            contextSalt,
            true,
            comparator == UserComparator::SensitiveTextNotStartsWithAnyOf
        );
    }

    case UserComparator::TextEndsWithAnyOf:
    case UserComparator::TextNotEndsWithAnyOf: {
        string text;
        const auto& textRef = getUserAttributeValueAsText(userAttributeName, *userAttributeValuePtr, condition, context.key, text);

        return evaluateTextSliceEqualsAnyOf(
            textRef,
            condition.comparisonValue,
            false,
            comparator == UserComparator::TextNotEndsWithAnyOf
        );
    }

    case UserComparator::SensitiveTextEndsWithAnyOf:
    case UserComparator::SensitiveTextNotEndsWithAnyOf: {
        string text;
        const auto& textRef = getUserAttributeValueAsText(userAttributeName, *userAttributeValuePtr, condition, context.key, text);

        return evaluateSensitiveTextSliceEqualsAnyOf(
            textRef,
            condition.comparisonValue,
            ensureConfigJsonSalt(context.setting.configJsonSalt),
            contextSalt,
            false,
            comparator == UserComparator::SensitiveTextNotEndsWithAnyOf
        );
    }

    case UserComparator::TextContainsAnyOf:
    case UserComparator::TextNotContainsAnyOf: {
        string text;
        const auto& textRef = getUserAttributeValueAsText(userAttributeName, *userAttributeValuePtr, condition, context.key, text);

        return evaluateTextContainsAnyOf(
            textRef,
            condition.comparisonValue,
            comparator == UserComparator::TextNotContainsAnyOf
        );
    }

    case UserComparator::SemVerIsOneOf:
    case UserComparator::SemVerIsNotOneOf: {
        semver::version version;
        const auto versionPtrOrError = getUserAttributeValueAsSemVer(userAttributeName, *userAttributeValuePtr, condition, context.key, version);
        if (auto errorPtr = get_if<string>(&versionPtrOrError)) {
            return std::move(*const_cast<string*>(errorPtr));
        }

        return evaluateSemVerIsOneOf(
            *get<const semver::version*>(versionPtrOrError),
            condition.comparisonValue,
            comparator == UserComparator::SemVerIsNotOneOf
        );
    }

    case UserComparator::SemVerLess:
    case UserComparator::SemVerLessOrEquals:
    case UserComparator::SemVerGreater:
    case UserComparator::SemVerGreaterOrEquals: {
        semver::version version;
        const auto versionPtrOrError = getUserAttributeValueAsSemVer(userAttributeName, *userAttributeValuePtr, condition, context.key, version);
        if (auto errorPtr = get_if<string>(&versionPtrOrError)) {
            return std::move(*const_cast<string*>(errorPtr));
        }

        return evaluateSemVerRelation(
            *get<const semver::version*>(versionPtrOrError),
            comparator,
            condition.comparisonValue
        );
    }

    case UserComparator::NumberEquals:
    case UserComparator::NumberNotEquals:
    case UserComparator::NumberLess:
    case UserComparator::NumberLessOrEquals:
    case UserComparator::NumberGreater:
    case UserComparator::NumberGreaterOrEquals: {
        const auto numberOrError = getUserAttributeValueAsNumber(userAttributeName, *userAttributeValuePtr, condition, context.key);
        if (auto errorPtr = get_if<string>(&numberOrError)) {
            return std::move(*const_cast<string*>(errorPtr));
        }

        return evaluateNumberRelation(
            get<double>(numberOrError),
            comparator,
            condition.comparisonValue
        );
    }

    case UserComparator::DateTimeBefore:
    case UserComparator::DateTimeAfter: {
        const auto numberOrError = getUserAttributeValueAsUnixTimeSeconds(userAttributeName, *userAttributeValuePtr, condition, context.key);
        if (auto errorPtr = get_if<string>(&numberOrError)) {
            return std::move(*const_cast<string*>(errorPtr));
        }

        return evaluateDateTimeRelation(
            get<double>(numberOrError),
            condition.comparisonValue,
            comparator == UserComparator::DateTimeBefore
        );
    }

    case UserComparator::ArrayContainsAnyOf:
    case UserComparator::ArrayNotContainsAnyOf: {
        vector<string> array;
        const auto arrayPtrOrError = getUserAttributeValueAsStringArray(userAttributeName, *userAttributeValuePtr, condition, context.key, array);
        if (auto errorPtr = get_if<string>(&arrayPtrOrError)) {
            return std::move(*const_cast<string*>(errorPtr));
        }

        return evaluateArrayContainsAnyOf(
            *get<const vector<string>*>(arrayPtrOrError),
            condition.comparisonValue,
            comparator == UserComparator::ArrayNotContainsAnyOf
        );
    }

    case UserComparator::SensitiveArrayContainsAnyOf:
    case UserComparator::SensitiveArrayNotContainsAnyOf: {
        vector<string> array;
        const auto arrayPtrOrError = getUserAttributeValueAsStringArray(userAttributeName, *userAttributeValuePtr, condition, context.key, array);
        if (auto errorPtr = get_if<string>(&arrayPtrOrError)) {
            return std::move(*const_cast<string*>(errorPtr));
        }

        return evaluateSensitiveArrayContainsAnyOf(
            *get<const vector<string>*>(arrayPtrOrError),
            condition.comparisonValue,
            ensureConfigJsonSalt(context.setting.configJsonSalt),
            contextSalt,
            comparator == UserComparator::SensitiveArrayNotContainsAnyOf
        );
    }

    default:
        throw runtime_error("Comparison operator is invalid.");
    }
}

bool RolloutEvaluator::evaluateTextEquals(const std::string& text, const UserConditionComparisonValue& comparisonValue, bool negate) const {
    const auto& text2 = ensureComparisonValue<string>(comparisonValue);

    return (text == text2) ^ negate;
}

bool RolloutEvaluator::evaluateSensitiveTextEquals(const std::string& text, const UserConditionComparisonValue& comparisonValue, const std::string& configJsonSalt, const std::string& contextSalt, bool negate) const {
    const auto& hash2 = ensureComparisonValue<string>(comparisonValue);

    const auto hash = hashComparisonValue(text, configJsonSalt, contextSalt);

    return (hash == hash2) ^ negate;
}

bool RolloutEvaluator::evaluateTextIsOneOf(const std::string& text, const UserConditionComparisonValue& comparisonValue, bool negate) const {
    const auto& comparisonValues = ensureComparisonValue<vector<string>>(comparisonValue);

    for (const auto& comparisonValue : comparisonValues) {
        if (text == comparisonValue) {
            return !negate;
        }
    }

    return negate;
}

bool RolloutEvaluator::evaluateSensitiveTextIsOneOf(const std::string& text, const UserConditionComparisonValue& comparisonValue, const std::string& configJsonSalt, const std::string& contextSalt, bool negate) const {
    const auto& comparisonValues = ensureComparisonValue<vector<string>>(comparisonValue);

    const auto hash = hashComparisonValue(text, configJsonSalt, contextSalt);

    for (const auto& comparisonValue : comparisonValues) {
        if (hash == comparisonValue) {
            return !negate;
        }
    }

    return negate;
}

bool RolloutEvaluator::evaluateTextSliceEqualsAnyOf(const std::string& text, const UserConditionComparisonValue& comparisonValue, bool startsWith, bool negate) const {
    const auto& comparisonValues = ensureComparisonValue<vector<string>>(comparisonValue);

    for (const auto& comparisonValue : comparisonValues) {
        const auto success = startsWith ? starts_with(text, comparisonValue) : ends_with(text, comparisonValue);

        if (success) {
            return !negate;
        }
    }

    return negate;
}

bool RolloutEvaluator::evaluateSensitiveTextSliceEqualsAnyOf(const std::string& text, const UserConditionComparisonValue& comparisonValue, const std::string& configJsonSalt, const std::string& contextSalt, bool startsWith, bool negate) const {
    const auto& comparisonValues = ensureComparisonValue<vector<string>>(comparisonValue);

    const auto textLength = text.size();

    for (const auto& comparisonValue : comparisonValues) {
        const auto index = comparisonValue.find('_');

        size_t sliceLength;
        if (index == string::npos
            || (sliceLength = integer_from_string(comparisonValue.substr(0, index)).value_or(-1)) < 0) {
            throw runtime_error("Comparison value is missing or invalid.");
        }

        const string hash2(comparisonValue.substr(index + 1));
        if (hash2.empty()) {
            throw runtime_error("Comparison value is missing or invalid.");
        }

        if (textLength < sliceLength) {
            continue;
        }

        const auto slice = startsWith ? text.substr(0, sliceLength) : text.substr(textLength - sliceLength);

        const auto hash = hashComparisonValue(slice, configJsonSalt, contextSalt);
        if (hash == hash2) {
            return !negate;
        }
    }

    return negate;
}

bool RolloutEvaluator::evaluateTextContainsAnyOf(const std::string& text, const UserConditionComparisonValue& comparisonValue, bool negate) const {
    const auto& comparisonValues = ensureComparisonValue<vector<string>>(comparisonValue);

    for (const auto& comparisonValue : comparisonValues) {
        if (contains(text, comparisonValue)) {
            return !negate;
        }
    }

    return negate;
}

bool RolloutEvaluator::evaluateSemVerIsOneOf(const semver::version& version, const UserConditionComparisonValue& comparisonValue, bool negate) const {
    const auto& comparisonValues = ensureComparisonValue<vector<string>>(comparisonValue);

    auto result = false;

    for (auto comparisonValue : comparisonValues) {
        // NOTE: Previous versions of the evaluation algorithm ignore empty comparison values.
        // We keep this behavior for backward compatibility.
        if (comparisonValue.empty()) {
            continue;
        }

        semver::version version2;
        try {
            trim(comparisonValue);
            version2 = semver::version::parse(comparisonValue);
        }
        catch (const semver::semver_exception&) {
            // NOTE: Previous versions of the evaluation algorithm ignored invalid comparison values.
            // We keep this behavior for backward compatibility.
            return false;
        }

        if (!result && version == version2) {
            // NOTE: Previous versions of the evaluation algorithm require that
            // none of the comparison values are empty or invalid, that is, we can't stop when finding a match.
            // We keep this behavior for backward compatibility.
            result = true;
        }
    }

    return result ^ negate;
}

bool RolloutEvaluator::evaluateSemVerRelation(const semver::version& version, UserComparator comparator, const UserConditionComparisonValue& comparisonValue) const {
    auto comparisonValueStr = ensureComparisonValue<string>(comparisonValue);

    semver::version version2;
    try {
        trim(comparisonValueStr);
        version2 = semver::version::parse(comparisonValueStr);
    }
    catch (const semver::semver_exception&) {
        // NOTE: Previous versions of the evaluation algorithm ignored invalid comparison values.
        // We keep this behavior for backward compatibility.
        return false;
    }

    switch (comparator) {
    case UserComparator::SemVerLess: return version < version2;
    case UserComparator::SemVerLessOrEquals: return version <= version2;
    case UserComparator::SemVerGreater: return version > version2;
    case UserComparator::SemVerGreaterOrEquals: return version >= version2;
    default: throw logic_error("Non-exhaustive switch.");
    }
}

bool RolloutEvaluator::evaluateNumberRelation(double number, UserComparator comparator, const UserConditionComparisonValue& comparisonValue) const {
    const auto number2 = ensureComparisonValue<double>(comparisonValue);

    switch (comparator) {
    case UserComparator::NumberEquals: return number == number2;
    case UserComparator::NumberNotEquals: return number != number2;
    case UserComparator::NumberLess: return number < number2;
    case UserComparator::NumberLessOrEquals: return number <= number2;
    case UserComparator::NumberGreater: return number > number2;
    case UserComparator::NumberGreaterOrEquals: return number >= number2;
    default: throw logic_error("Non-exhaustive switch.");
    }
}

bool RolloutEvaluator::evaluateDateTimeRelation(double number, const UserConditionComparisonValue& comparisonValue, bool before) const
{
    const auto number2 = ensureComparisonValue<double>(comparisonValue);

    return before ? number < number2 : number > number2;
}

bool RolloutEvaluator::evaluateArrayContainsAnyOf(const std::vector<std::string>& array, const UserConditionComparisonValue& comparisonValue, bool negate) const {
    const auto& comparisonValues = ensureComparisonValue<vector<string>>(comparisonValue);

    for (const auto& text : array) {
        for (const auto& comparisonValue : comparisonValues) {
            if (text == comparisonValue) {
                return !negate;
            }
        }
    }

    return negate;
}

bool RolloutEvaluator::evaluateSensitiveArrayContainsAnyOf(const std::vector<std::string>& array, const UserConditionComparisonValue& comparisonValue, const std::string& configJsonSalt, const std::string& contextSalt, bool negate) const {
    const auto& comparisonValues = ensureComparisonValue<vector<string>>(comparisonValue);

    for (const auto& text : array) {
        const auto hash = hashComparisonValue(text, configJsonSalt, contextSalt);

        for (const auto& comparisonValue : comparisonValues) {
            if (hash == comparisonValue) {
                return !negate;
            }
        }
    }

    return negate;
}

bool RolloutEvaluator::evaluatePrerequisiteFlagCondition(const PrerequisiteFlagCondition& condition, EvaluateContext& context) const {
    const auto& logBuilder = context.logBuilder;

    if (logBuilder) logBuilder->appendPrerequisiteFlagCondition(condition, context.settings);

    const auto& prerequisiteFlagKey = condition.prerequisiteFlagKey;

    assert(context.settings);
    const auto it = context.settings->find(prerequisiteFlagKey);
    if (it == context.settings->end()) {
        throw runtime_error("Prerequisite flag is missing or invalid.");
    }
    const auto& prerequisiteFlag = it->second;

    const auto& comparisonValue = condition.comparisonValue;
    if (holds_alternative<nullopt_t>(comparisonValue)) {
        throw runtime_error("Comparison value is missing or invalid.");
    }

    const auto expectedSettingType = comparisonValue.getSettingType();
    if (!prerequisiteFlag.hasInvalidType() && prerequisiteFlag.type != expectedSettingType) {
        string str;
        throw runtime_error(string_format("Type mismatch between comparison value '%s' and prerequisite flag '%s'.",
            formatSettingValue(comparisonValue, str).c_str(), prerequisiteFlagKey.c_str()));
    }

    auto visitedFlags = context.getVisitedFlags();
    visitedFlags->push_back(context.key);
    if (find(visitedFlags->begin(), visitedFlags->end(), prerequisiteFlagKey) != visitedFlags->end()) {
        visitedFlags->push_back(prerequisiteFlagKey);
        ostringstream ss;
        ss << "Circular dependency detected between the following depending flags: ";
        append_stringlist(ss, *visitedFlags, 0, nullopt, " -> ");
        throw runtime_error(ss.str());
    }

    auto prerequisiteFlagContext = EvaluateContext::forPrerequisiteFlag(prerequisiteFlagKey, prerequisiteFlag, context);

    if (logBuilder) {
        logBuilder->newLine('(')
            .increaseIndent()
            .newLine().appendFormat("Evaluating prerequisite flag '%s':", prerequisiteFlagKey.c_str());
    }

    const auto prerequisiteFlagEvaluateResult = evaluateSetting(prerequisiteFlagContext);

    visitedFlags->pop_back();

    const auto prerequisiteFlagValue = *prerequisiteFlagEvaluateResult.selectedValue.value.toValueChecked(expectedSettingType);
    // At this point it's ensured that the return value of the prerequisite flag is compatible with the comparison value.

    const auto comparator = condition.comparator;

    bool result;
    switch (comparator) {
    case PrerequisiteFlagComparator::Equals:
        if (const auto& comparisonValuePtr = get_if<string>(&comparisonValue); comparisonValuePtr) result = *comparisonValuePtr == get<string>(prerequisiteFlagValue);
        else result = *static_cast<std::optional<Value>>(comparisonValue) == prerequisiteFlagValue;
        break;

    case PrerequisiteFlagComparator::NotEquals:
        if (const auto& comparisonValuePtr = get_if<string>(&comparisonValue); comparisonValuePtr) result = *comparisonValuePtr != get<string>(prerequisiteFlagValue);
        else result = *static_cast<std::optional<Value>>(comparisonValue) != prerequisiteFlagValue;
        break;

    default:
        throw runtime_error("Comparison operator is invalid.");
    }

    if (logBuilder) {
        string str;
        logBuilder->newLine().appendFormat("Prerequisite flag evaluation result: '%s'.", formatSettingValue(prerequisiteFlagEvaluateResult.selectedValue.value, str).c_str())
            .newLine("Condition (")
            .appendPrerequisiteFlagCondition(condition, context.settings)
            .append(") evaluates to ").appendConditionResult(result).append('.')
            .decreaseIndent()
            .newLine(')');
    }

    return result;
}

RolloutEvaluator::SuccessOrError RolloutEvaluator::evaluateSegmentCondition(const SegmentCondition& condition, EvaluateContext& context) const {
    const auto& logBuilder = context.logBuilder;

    const auto& segments = context.setting.segments;
    if (logBuilder) logBuilder->appendSegmentCondition(condition, segments);

    if (!context.user) {
        if (!context.isMissingUserObjectLogged) {
            logUserObjectIsMissing(context.key);
            context.isMissingUserObjectLogged = true;
        }

        return string(kMissingUserObjectError);
    }

    const auto segmentIndex = condition.segmentIndex;
    if (segmentIndex < 0 || (segments ? segments->size() : 0) <= segmentIndex) {
        throw runtime_error("Segment reference is invalid.");
    }

    const auto& segment = (*segments)[segmentIndex];

    const auto& segmentName = segment.name;
    if (segmentName.empty()) {
        throw runtime_error("Segment name is missing.");
    }

    if (logBuilder) {
        logBuilder->newLine('(')
            .increaseIndent()
            .newLine().appendFormat("Evaluating segment '%s':", segmentName.c_str());
    }

    const auto& conditions = segment.conditions;
    const auto conditionAccessor = std::function([](const UserCondition& condition) -> Condition { return condition; });

    auto result = evaluateConditions(conditions, conditionAccessor, nullptr, segmentName, context);
    SegmentComparator segmentResult;

    if (!holds_alternative<string>(result)) {
        segmentResult = get<bool>(result) ? SegmentComparator::IsIn : SegmentComparator::IsNotIn;
        const auto comparator = condition.comparator;

        switch (comparator) {
        case SegmentComparator::IsIn:
            break;

        case SegmentComparator::IsNotIn:
            result = !get<bool>(result);
            break;

        default:
            throw runtime_error("Comparison operator is invalid.");
        }
    } else {
        segmentResult = static_cast<SegmentComparator>(-1);
    }

    if (logBuilder) {
        logBuilder->newLine("Segment evaluation result: ");

        if (!holds_alternative<string>(result)) {
            logBuilder->appendFormat("User %s", getSegmentComparatorText(segmentResult));
        } else {
            logBuilder->append(get<string>(result));
        }
        logBuilder->append('.');

        logBuilder->newLine("Condition (").appendSegmentCondition(condition, segments).append(')');
        (!holds_alternative<string>(result)
            ? logBuilder->append(" evaluates to ").appendConditionResult(get<bool>(result))
            : logBuilder->append(" failed to evaluate"))
            .append('.');

        logBuilder
            ->decreaseIndent()
            .newLine(')');
    }

    return result;
}

std::string RolloutEvaluator::userAttributeValueToString(const ConfigCatUser::AttributeValue& attributeValue) {
    return visit([](auto&& alt) -> string {
        using T = decay_t<decltype(alt)>;
        if constexpr (is_same_v<T, string>) {
            return alt;
        } else if constexpr (is_same_v<T, double>) {
            return number_to_string(alt);
        } else if constexpr (is_same_v<T, date_time_t>) {
            const auto unixTimeSeconds = datetime_to_unixtimeseconds(alt);
            return number_to_string(unixTimeSeconds ? *unixTimeSeconds : NAN);
        } else if constexpr (is_same_v<T, vector<string>>) {
            nlohmann::json j = alt;
            return j.dump();
        } else {
            static_assert(always_false_v<T>, "Non-exhaustive visitor.");
        }
    }, attributeValue);
}

const std::string& RolloutEvaluator::getUserAttributeValueAsText(const std::string& attributeName, const ConfigCatUser::AttributeValue& attributeValue,
    const UserCondition& condition, const std::string& key, std::string& text) const {

    if (const auto textPtr = get_if<string>(&attributeValue); textPtr) {
        return *textPtr;
    }

    text = userAttributeValueToString(attributeValue);
    logUserObjectAttributeIsAutoConverted(formatUserCondition(condition), key, attributeName, text);
    return text;
}

std::variant<const semver::version*, std::string> RolloutEvaluator::getUserAttributeValueAsSemVer(const std::string& attributeName, const ConfigCatUser::AttributeValue& attributeValue,
    const UserCondition& condition, const std::string& key, semver::version& version) const {

    if (const auto textPtr = get_if<string>(&attributeValue); textPtr) {
        try {
            string text(*textPtr);
            trim(text);
            version = semver::version::parse(text);
            return &version;
        }
        catch (const semver::semver_exception&) {
            /* intentional no-op */
        }
    }

    return handleInvalidUserAttribute(condition, key, attributeName,
        string_format("'%s' is not a valid semantic version", userAttributeValueToString(attributeValue).c_str()));
}

std::variant<double, std::string> RolloutEvaluator::getUserAttributeValueAsNumber(const std::string& attributeName, const ConfigCatUser::AttributeValue& attributeValue,
    const UserCondition& condition, const std::string& key) const {

    if (const auto numberPtr = get_if<double>(&attributeValue); numberPtr) {
        return *numberPtr;
    } else if (const auto textPtr = get_if<string>(&attributeValue); textPtr) {
        auto number = number_from_string(*textPtr);
        if (number) return *number;
    }

    return handleInvalidUserAttribute(condition, key, attributeName,
        string_format("'%s' is not a valid decimal number", userAttributeValueToString(attributeValue).c_str()));
}

std::variant<double, std::string> RolloutEvaluator::getUserAttributeValueAsUnixTimeSeconds(const std::string& attributeName, const ConfigCatUser::AttributeValue& attributeValue,
    const UserCondition& condition, const std::string& key) const {

    if (const auto dateTimePtr = get_if<date_time_t>(&attributeValue); dateTimePtr) {
        const auto unixTimeSeconds = datetime_to_unixtimeseconds(*dateTimePtr);
        if (unixTimeSeconds) return *unixTimeSeconds;
    } else if (const auto numberPtr = get_if<double>(&attributeValue); numberPtr) {
        return *numberPtr;
    } else if (const auto textPtr = get_if<string>(&attributeValue); textPtr) {
        auto number = number_from_string(*textPtr);
        if (number) return *number;
    }

    return handleInvalidUserAttribute(condition, key, attributeName,
        string_format("'%s' is not a valid Unix timestamp (number of seconds elapsed since Unix epoch)", userAttributeValueToString(attributeValue).c_str()));
}

std::variant<const std::vector<std::string>*, std::string> RolloutEvaluator::getUserAttributeValueAsStringArray(const std::string& attributeName, const ConfigCatUser::AttributeValue& attributeValue,
    const UserCondition& condition, const std::string& key, std::vector<std::string>& array) const {

    if (const auto arrayPtr = get_if<vector<string>>(&attributeValue); arrayPtr) {
        return arrayPtr;
    } else if (const auto textPtr = get_if<string>(&attributeValue); textPtr) {
        try {
            auto j = nlohmann::json::parse(*textPtr, nullptr, true, false);
            j.get_to(array);
            return &array;
        }
        catch (...) { /* intentional no-op */ }
    }

    return handleInvalidUserAttribute(condition, key, attributeName,
        string_format("'%s' is not a valid string array", userAttributeValueToString(attributeValue).c_str()));
}

void RolloutEvaluator::logUserObjectIsMissing(const std::string& key) const {
    LOG_WARN(3001)
        << "Cannot evaluate targeting rules and % options for setting '" << key << "' (User Object is missing). "
        << "You should pass a User Object to the evaluation methods like `getValue()` in order to make targeting work properly. "
        << "Read more: https://configcat.com/docs/advanced/user-object/";
}

void RolloutEvaluator::logUserObjectAttributeIsMissingPercentage(const std::string& key, const std::string& attributeName) const {
    LOG_WARN(3003)
        << "Cannot evaluate % options for setting '" << key << "' (the User." << attributeName << " attribute is missing). "
        << "You should set the User." << attributeName << " attribute in order to make targeting work properly. "
        << "Read more: https://configcat.com/docs/advanced/user-object/";
}

void RolloutEvaluator::logUserObjectAttributeIsMissingCondition(const std::string& condition, const std::string& key, const std::string& attributeName) const {
    LOG_WARN(3003)
        << "Cannot evaluate condition (" << condition << ") for setting '" << key << "' (the User." << attributeName << " attribute is missing). "
        << "You should set the User." << attributeName << " attribute in order to make targeting work properly. "
        << "Read more: https://configcat.com/docs/advanced/user-object/";
}

void RolloutEvaluator::logUserObjectAttributeIsInvalid(const std::string& condition, const std::string& key, const std::string& reason, const std::string& attributeName) const {
    LOG_WARN(3004)
        << "Cannot evaluate condition (" << condition << ") for setting '" << key << "' (" << reason << "). "
        << "Please check the User." << attributeName << " attribute and make sure that its value corresponds to the comparison operator.";
}

void RolloutEvaluator::logUserObjectAttributeIsAutoConverted(const std::string& condition, const std::string& key, const std::string& attributeName, const std::string& attributeValue) const {
    LOG_WARN(3005)
        << "Evaluation of condition (" << condition << ") for setting '" << key << "' may not produce the expected result "
        << "(the User." << attributeName << " attribute is not a string value, thus it was automatically converted to the string value '" << attributeValue << "'). "
        << "Please make sure that using a non-string value was intended.";
}

std::string RolloutEvaluator::handleInvalidUserAttribute(const UserCondition& condition, const std::string& key, const std::string& attributeName, const std::string& reason) const {
    logUserObjectAttributeIsInvalid(formatUserCondition(condition), key, reason, attributeName);

    return string_format(kInvalidUserAttributeError, attributeName.c_str(), reason.c_str());
}

} // namespace configcat
