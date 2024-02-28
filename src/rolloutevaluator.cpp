#include <assert.h>
#include <exception>
#include <ostream> // must be imported before <semver/semver.hpp>

#include <hash-library/sha1.h>
#include <hash-library/sha256.h>
#include <nlohmann/json.hpp>
#include <semver/semver.hpp>

#include "configcatlogger.h"
#include "rolloutevaluator.h"
#include "utils.h"

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

    static string hashComparisonValue(SHA256& sha256, const string& value, const string& configJsonSalt, const string& contextSalt) {
        return sha256(value + configJsonSalt + contextSalt);
    }

    RolloutEvaluator::RolloutEvaluator(const std::shared_ptr<ConfigCatLogger>& logger) :
        logger(logger),
        sha1(make_unique<SHA1>()),
        sha256(make_unique<SHA256>()) {
    }

    EvaluateResult RolloutEvaluator::evaluate(const std::optional<Value>& defaultValue, EvaluateContext& context, std::optional<Value>& returnValue) const {
        auto& logBuilder = context.logBuilder;

        // Building the evaluation log is expensive, so let's not do it if it wouldn't be logged anyway.
        if (this->logger->isEnabled(LogLevel::LOG_LEVEL_INFO)) {
            logBuilder = make_unique<EvaluateLogBuilder>();

            logBuilder->appendFormat("Evaluating '%s'", context.key.c_str());

            if (context.user) {
                logBuilder->appendFormat(" for User '%s'", context.user->toJson().c_str());
            }

            logBuilder->increaseIndent();
        }

        const auto log = [](
            const std::optional<Value>& returnValue,
            const shared_ptr<ConfigCatLogger>& logger,
            const unique_ptr<EvaluateLogBuilder>& logBuilder
            ) {
                if (logBuilder) {
                    logBuilder->newLine().appendFormat("Returning '%s'.", returnValue ? returnValue->toString().c_str() : "");
                    logBuilder->decreaseIndent();

                    logger->log(LogLevel::LOG_LEVEL_INFO, 5000, logBuilder->toString());
                }
            };

        exception_ptr eptr;
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
                        "Learn more: https://configcat.com/docs/sdk-reference/dotnet/#setting-type-mapping",
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
            eptr = current_exception();
            if (logBuilder) logBuilder->resetIndent().increaseIndent();

            log(defaultValue, logger, logBuilder);
            rethrow_exception(eptr);
        }
    }

    EvaluateResult RolloutEvaluator::evaluateSetting(EvaluateContext& context) const {
        const auto& targetingRules = context.setting.targetingRules;
        if (!targetingRules.empty()) {
            auto evaluateResult = evaluateTargetingRules(targetingRules, context);
            if (evaluateResult) {
                return move(*evaluateResult);
            }
        }

        const auto& percentageOptions = context.setting.percentageOptions;
        if (!percentageOptions.empty()) {
            auto evaluateResult = evaluatePercentageOptions(percentageOptions, nullptr, context);
            if (evaluateResult) {
                return move(*evaluateResult);
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

                return move(*evaluateResult);
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
            if (logBuilder) logBuilder->newLine("Skipping %% options because the User Object is missing.");

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
        const auto userAttributeValue = move(userAttributeValuePtr ? string() : userAttributeValueToString(*percentageOptionsAttributeValuePtr));

        auto sha1 = (*this->sha1)(context.key + (userAttributeValuePtr ? *userAttributeValuePtr : userAttributeValue));
        const auto hashValue = std::stoul(sha1.erase(7), nullptr, 16) % 100;

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
                logBuilder->newLine().appendFormat("- Hash value %d selects %% option %d (%d%), '%s'.",
                    hashValue, optionNumber, percentage, formatSettingValue(percentageOption.value).c_str());
            }

            return EvaluateResult{ percentageOption, matchedTargetingRule, &percentageOption };
        }

        throw runtime_error("Sum of percentage option percentages are less than 100.");
    }

    template <typename ConditionType>
    RolloutEvaluator::SuccessOrError RolloutEvaluator::evaluateConditions(const std::vector<ConditionType>& conditions, const std::function<const Condition& (const ConditionType&)>& conditionAccessor,
        const TargetingRule* targetingRule, const std::string& contextSalt, EvaluateContext& context) const {

        RolloutEvaluator::SuccessOrError result = true;

        const auto& logBuilder = context.logBuilder;
        auto newLineBeforeThen = false;

        if (logBuilder) logBuilder->newLine("- ");

        auto i = 0;
        for (const auto& it : conditions) {
            const Condition& condition = conditionAccessor(it);

            if (logBuilder) {
                if (i == 0) {
                    logBuilder->append("IF ")
                        .increaseIndent();
                }
                else {
                    logBuilder->increaseIndent()
                        .newLine("AND ");
                }
            }

            if (const auto userConditionPtr = get_if<UserCondition>(&condition); userConditionPtr) {
                result = evaluateUserCondition(*userConditionPtr, contextSalt, context);
                newLineBeforeThen = conditions.size() > 1;
            }
            else if (const auto prerequisiteFlagConditionPtr = get_if<PrerequisiteFlagCondition>(&condition); prerequisiteFlagConditionPtr) {
                throw runtime_error("Not implemented."); // TODO
            }
            else if (const auto segmentConditionPtr = get_if<SegmentCondition>(&condition); segmentConditionPtr) {
                throw runtime_error("Not implemented."); // TODO
            }
            else {
                throw runtime_error("Condition is missing or invalid.");
            }

            const auto successPtr = get_if<bool>(&result);
            const auto success = successPtr && *successPtr;

            if (logBuilder) {
                if (targetingRule || conditions.size() > 1) {
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
                this->logUserObjectIsMissing(context.key);
                context.isMissingUserObjectLogged = true;
            }

            return string(kMissingUserObjectError);
        }

        const auto& userAttributeName = condition.comparisonAttribute;
        const auto userAttributeValuePtr = context.user->getAttribute(userAttributeName);

        if (const string* textPtr; !userAttributeValuePtr || (textPtr = get_if<string>(userAttributeValuePtr)) && textPtr->empty()) {
            this->logUserObjectAttributeIsMissingCondition(formatUserCondition(condition), context.key, userAttributeName);

            return string_format(kMissingUserAttributeError, userAttributeName.c_str());
        }

        const auto comparator = condition.comparator;

        switch (comparator) {
        case UserComparator::TextEquals:
        case UserComparator::TextNotEquals:
            throw runtime_error("Not implemented."); // TODO

        case UserComparator::SensitiveTextEquals:
        case UserComparator::SensitiveTextNotEquals:
            throw runtime_error("Not implemented."); // TODO

        case UserComparator::TextIsOneOf:
        case UserComparator::TextIsNotOneOf: {
            const auto textPtr = get_if<string>(userAttributeValuePtr);
            const auto text = move(textPtr ? string() : getUserAttributeValueAsText(userAttributeName, *userAttributeValuePtr, condition, context.key));

            return evaluateTextIsOneOf(
                textPtr ? *textPtr : text,
                condition.comparisonValue,
                UserComparator::TextIsNotOneOf == comparator
            );
        }

        case UserComparator::SensitiveTextIsOneOf:
        case UserComparator::SensitiveTextIsNotOneOf: {
            const auto textPtr = get_if<string>(userAttributeValuePtr);
            const auto text = move(textPtr ? string() : getUserAttributeValueAsText(userAttributeName, *userAttributeValuePtr, condition, context.key));

            return evaluateSensitiveTextIsOneOf(
                textPtr ? *textPtr : text,
                condition.comparisonValue,
                ensureConfigJsonSalt(context.setting.configJsonSalt),
                contextSalt,
                UserComparator::SensitiveTextIsNotOneOf == comparator
            );
        }

        case UserComparator::TextStartsWithAnyOf:
        case UserComparator::TextNotStartsWithAnyOf:
            throw runtime_error("Not implemented."); // TODO

        case UserComparator::SensitiveTextStartsWithAnyOf:
        case UserComparator::SensitiveTextNotStartsWithAnyOf:
            throw runtime_error("Not implemented."); // TODO

        case UserComparator::TextEndsWithAnyOf:
        case UserComparator::TextNotEndsWithAnyOf:
            throw runtime_error("Not implemented."); // TODO

        case UserComparator::SensitiveTextEndsWithAnyOf:
        case UserComparator::SensitiveTextNotEndsWithAnyOf:
            throw runtime_error("Not implemented."); // TODO

        case UserComparator::TextContainsAnyOf:
        case UserComparator::TextNotContainsAnyOf: {
            const auto textPtr = get_if<string>(userAttributeValuePtr);
            const auto text = move(textPtr ? string() : getUserAttributeValueAsText(userAttributeName, *userAttributeValuePtr, condition, context.key));

            return evaluateTextContainsAnyOf(
                textPtr ? *textPtr : text,
                condition.comparisonValue,
                UserComparator::TextNotContainsAnyOf == comparator
            );
        }

        case UserComparator::SemVerIsOneOf:
        case UserComparator::SemVerIsNotOneOf: {
            const auto versionOrError = this->getUserAttributeValueAsSemVer(userAttributeName, *userAttributeValuePtr, condition, context.key);
            const auto versionPtr = get_if<semver::version>(&versionOrError);
            if (!versionPtr) {
                return get<string>(versionOrError);
            }

            return evaluateSemVerIsOneOf(
                *versionPtr,
                condition.comparisonValue,
                UserComparator::SemVerIsNotOneOf == comparator
            );
        }

        case UserComparator::SemVerLess:
        case UserComparator::SemVerLessOrEquals:
        case UserComparator::SemVerGreater:
        case UserComparator::SemVerGreaterOrEquals: {
            const auto versionOrError = this->getUserAttributeValueAsSemVer(userAttributeName, *userAttributeValuePtr, condition, context.key);
            const auto versionPtr = get_if<semver::version>(&versionOrError);
            if (!versionPtr) {
                return get<string>(versionOrError);
            }

            return evaluateSemVerRelation(
                *versionPtr,
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
            const auto numberOrError = this->getUserAttributeValueAsNumber(userAttributeName, *userAttributeValuePtr, condition, context.key);
            const auto numberPtr = get_if<double>(&numberOrError);
            if (!numberPtr) {
                return get<string>(numberOrError);
            }

            return evaluateNumberRelation(
                *numberPtr,
                comparator,
                condition.comparisonValue
            );
        }

        case UserComparator::DateTimeBefore:
        case UserComparator::DateTimeAfter:
            throw runtime_error("Not implemented."); // TODO

        case UserComparator::ArrayContainsAnyOf:
        case UserComparator::ArrayNotContainsAnyOf:
            throw runtime_error("Not implemented."); // TODO

        case UserComparator::SensitiveArrayContainsAnyOf:
        case UserComparator::SensitiveArrayNotContainsAnyOf:
            throw runtime_error("Not implemented."); // TODO

        default:
            throw runtime_error("Comparison operator is invalid.");
        }
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

        const auto hash = hashComparisonValue(*sha256, text, configJsonSalt, contextSalt);

        for (const auto& comparisonValue : comparisonValues) {
            if (hash == comparisonValue) {
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

    std::string RolloutEvaluator::userAttributeValueToString(const ConfigCatUser::AttributeValue& attributeValue) {
        return visit([](auto&& alt) -> string {
            using T = decay_t<decltype(alt)>;
            if constexpr (is_same_v<T, string>) {
                return alt;
            }
            else if constexpr (is_same_v<T, double>) {
                return numberToString(alt);
            }
            else if constexpr (is_same_v<T, date_time_t>) {
                const auto unixTimeSeconds = dateTimeToUnixTimeSeconds(alt);
                return numberToString(unixTimeSeconds ? *unixTimeSeconds : NAN);
            }
            else if constexpr (is_same_v<T, vector<string>>) {
                nlohmann::json j = alt;
                return j.dump();
            }
            else {
                static_assert(always_false_v<T>, "Non-exhaustive visitor.");
            }
        }, attributeValue);
    }

    std::string RolloutEvaluator::getUserAttributeValueAsText(const std::string& attributeName, const ConfigCatUser::AttributeValue& attributeValue, const UserCondition& condition, const std::string& key) const {
        if (const auto textPtr = get_if<string>(&attributeValue); textPtr) {
            return *textPtr;
        }

        string text = userAttributeValueToString(attributeValue);
        logUserObjectAttributeIsAutoConverted(formatUserCondition(condition), key, attributeName, text);
        return text;
    }

    std::variant<semver::version, std::string> RolloutEvaluator::getUserAttributeValueAsSemVer(const std::string& attributeName, const ConfigCatUser::AttributeValue& attributeValue, const UserCondition& condition, const std::string& key) const {
        if (const auto textPtr = get_if<string>(&attributeValue); textPtr) {
            try { 
                auto text = *textPtr;
                trim(text);
                return semver::version::parse(text);
            }
            catch (const semver::semver_exception&) {
                /* intentional no-op */
            }
        }

        return handleInvalidUserAttribute(condition, key, attributeName, string_format("'%s' is not a valid semantic version", userAttributeValueToString(attributeValue).c_str()));
    }

    std::variant<double, std::string> RolloutEvaluator::getUserAttributeValueAsNumber(const std::string& attributeName, const ConfigCatUser::AttributeValue& attributeValue, const UserCondition& condition, const std::string& key) const {
        if (const auto numberPtr = get_if<double>(&attributeValue); numberPtr) {
            return *numberPtr;
        }
        if (const auto textPtr = get_if<string>(&attributeValue); textPtr) {
            auto number = numberFromString(*textPtr);
            if (number) return *number;
        }

        return handleInvalidUserAttribute(condition, key, attributeName, string_format("'%s' is not a valid decimal number", userAttributeValueToString(attributeValue).c_str()));
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
            << "(the User." << attributeName << " attribute is not a string value, thus it was automatically converted to the string value '{" << attributeValue << "}'). "
            << "Please make sure that using a non-string value was intended.";
    }

    std::string RolloutEvaluator::handleInvalidUserAttribute(const UserCondition& condition, const std::string& key, const std::string& attributeName, const std::string& reason) const {
        logUserObjectAttributeIsInvalid(formatUserCondition(condition), key, reason, attributeName);

        return string_format(kInvalidUserAttributeError, attributeName.c_str(), reason.c_str());
    }

} // namespace configcat
