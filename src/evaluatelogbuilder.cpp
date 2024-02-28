#include <array>

#include "evaluatelogbuilder.h"
#include "utils.h"

using namespace std;

namespace configcat {

    static constexpr char kInvalidValuePlaceholder[] = "<invalid value>";
    static constexpr char kInvalidOperatorPlaceholder[] = "<invalid operator>";

    static constexpr char kValueText[] = "value";
    static constexpr char kValuesText[] = "values";

    static constexpr size_t kStringListMaxCount = 10;

    EvaluateLogBuilder& configcat::EvaluateLogBuilder::newLine() {
        // TODO: platform-specific line terminator?
        ss << endl;
        for (auto i = 0; i < this->indentLevel; i++) {
            ss << "  ";
        }
        return *this;
    }

    EvaluateLogBuilder& EvaluateLogBuilder::appendUserConditionCore(const std::string& comparisonAttribute, UserComparator comparator, const std::optional<std::string>& comparisonValue) {
        return appendFormat("User.%s %s '%s'",
            comparisonAttribute.c_str(),
            getUserComparatorText(comparator),
            comparisonValue ? comparisonValue->c_str() : kInvalidValuePlaceholder);
    }

    EvaluateLogBuilder& EvaluateLogBuilder::appendUserConditionString(const std::string& comparisonAttribute, UserComparator comparator, const UserConditionComparisonValue& comparisonValue, bool isSensitive) {
        const auto comparisonValuePtr = get_if<string>(&comparisonValue);
        if (!comparisonValuePtr) {
            return appendUserConditionCore(comparisonAttribute, comparator, nullopt);
        }

        return appendUserConditionCore(comparisonAttribute, comparator, isSensitive ? *comparisonValuePtr : "<hashed value>");
    }

    EvaluateLogBuilder& EvaluateLogBuilder::appendUserConditionStringList(const std::string& comparisonAttribute, UserComparator comparator, const UserConditionComparisonValue& comparisonValue, bool isSensitive) {
        const auto comparisonValuesPtr = get_if<vector<string>>(&comparisonValue);
        if (!comparisonValuesPtr) {
            return appendUserConditionCore(comparisonAttribute, comparator, nullopt);
        }

        if (isSensitive) {
            const auto comparisonValueCount = comparisonValuesPtr->size();
            return appendFormat("User.%s %s [<%d hashed %s>]",
                comparisonAttribute.c_str(),
                getUserComparatorText(comparator),
                comparisonValueCount,
                comparisonValueCount == 1 ? kValueText : kValuesText);
        }
        else {
            appendFormat("User.%s %s [",
                comparisonAttribute.c_str(),
                getUserComparatorText(comparator));

            appendStringList(this->ss, *comparisonValuesPtr, kStringListMaxCount, [](size_t count) {
                return string_format(", ... <%d more %s>", count, count ? kValueText : kValuesText);
            });

            return append("]");
        }
    }

    EvaluateLogBuilder& EvaluateLogBuilder::appendUserConditionNumber(const std::string& comparisonAttribute, UserComparator comparator, const UserConditionComparisonValue& comparisonValue, bool isDateTime = false) {
        const auto comparisonValuePtr = get_if<double>(&comparisonValue);
        if (!comparisonValuePtr) {
            return appendUserConditionCore(comparisonAttribute, comparator, nullopt);
        }

        if (isDateTime) {
            if (const auto tp = dateTimeFromUnixTimeSeconds(*comparisonValuePtr); tp) {
                return appendFormat("User.%s %s '%g' (%s UTC)",
                    comparisonAttribute.c_str(),
                    getUserComparatorText(comparator),
                    *comparisonValuePtr,
                    formatDateTimeISO(*tp).c_str());
            }
        }

        return appendFormat("User.%s %s '%g'",
            comparisonAttribute.c_str(),
            getUserComparatorText(comparator),
            *comparisonValuePtr);
    }

    EvaluateLogBuilder& EvaluateLogBuilder::appendUserCondition(UserCondition condition) {       
        const auto& comparisonAttribute = condition.comparisonAttribute;
        const auto comparator = condition.comparator;
        const auto& comparisonValue = condition.comparisonValue;

        switch (comparator) {
        case UserComparator::TextIsOneOf:
        case UserComparator::TextIsNotOneOf:
        case UserComparator::TextContainsAnyOf:
        case UserComparator::TextNotContainsAnyOf:
        case UserComparator::SemVerIsOneOf:
        case UserComparator::SemVerIsNotOneOf:
        case UserComparator::TextStartsWithAnyOf:
        case UserComparator::TextNotStartsWithAnyOf:
        case UserComparator::TextEndsWithAnyOf:
        case UserComparator::TextNotEndsWithAnyOf:
        case UserComparator::ArrayContainsAnyOf:
        case UserComparator::ArrayNotContainsAnyOf:
            return appendUserConditionStringList(comparisonAttribute, comparator, comparisonValue, false);

        case UserComparator::SemVerLess:
        case UserComparator::SemVerLessOrEquals:
        case UserComparator::SemVerGreater:
        case UserComparator::SemVerGreaterOrEquals:
        case UserComparator::TextEquals:
        case UserComparator::TextNotEquals:
            return appendUserConditionString(comparisonAttribute, comparator, comparisonValue, false);

        case UserComparator::NumberEquals:
        case UserComparator::NumberNotEquals:
        case UserComparator::NumberLess:
        case UserComparator::NumberLessOrEquals:
        case UserComparator::NumberGreater:
        case UserComparator::NumberGreaterOrEquals:
            return appendUserConditionNumber(comparisonAttribute, comparator, comparisonValue);

        case UserComparator::SensitiveTextIsOneOf:
        case UserComparator::SensitiveTextIsNotOneOf:
        case UserComparator::SensitiveTextStartsWithAnyOf:
        case UserComparator::SensitiveTextNotStartsWithAnyOf:
        case UserComparator::SensitiveTextEndsWithAnyOf:
        case UserComparator::SensitiveTextNotEndsWithAnyOf:
        case UserComparator::SensitiveArrayContainsAnyOf:
        case UserComparator::SensitiveArrayNotContainsAnyOf:
            return appendUserConditionStringList(comparisonAttribute, comparator, comparisonValue, true);

        case UserComparator::DateTimeBefore:
        case UserComparator::DateTimeAfter:
            return appendUserConditionNumber(comparisonAttribute, comparator, comparisonValue, true);

        case UserComparator::SensitiveTextEquals:
        case UserComparator::SensitiveTextNotEquals:
            return appendUserConditionString(comparisonAttribute, comparator, comparisonValue, true);

        default:
            return appendUserConditionCore(comparisonAttribute, comparator, formatUserConditionComparisonValue(comparisonValue));
        }
    }

    EvaluateLogBuilder& EvaluateLogBuilder::appendConditionConsequence(bool result) {
        append(" => ").appendConditionResult(result);

        return result ? *this : append(", skipping the remaining AND conditions");
    }

    EvaluateLogBuilder& EvaluateLogBuilder::appendTargetingRuleThenPart(const TargetingRule& targetingRule, SettingType settingType, bool newLine) {
        (newLine ? this->newLine() : append(" "))
            .append("THEN");

        const auto simpleValuePtr = get_if<SettingValueContainer>(&targetingRule.then);
        if (simpleValuePtr) {
            return appendFormat(" '%s'", formatSettingValue(simpleValuePtr->value).c_str());
        }
        else {
            return append(" % options");
        }
    }

    EvaluateLogBuilder& EvaluateLogBuilder::appendTargetingRuleConsequence(const TargetingRule& targetingRule, SettingType settingType, const std::variant<bool, std::string>& isMatchOrError, bool newLine) {
        increaseIndent();

        const auto isMatchPtr = get_if<bool>(&isMatchOrError);
        appendTargetingRuleThenPart(targetingRule, settingType, newLine)
            .append(" => ").append(isMatchPtr
                ? (*isMatchPtr ? "MATCH, applying rule" : "no match")
                : get<string>(isMatchOrError));

        return decreaseIndent();
    }

    const char* getSettingTypeText(SettingType settingType) {
        static constexpr std::array kTexts{
            "Boolean",
            "String",
            "Int",
            "Double",
        };

        return kTexts.at(static_cast<size_t>(settingType));
    }

    const char* getSettingValueTypeText(const SettingValue& settingValue) {
        static constexpr std::array kTexts{
            "std::nullopt",
            "bool",
            "std::string",
            "int32_t",
            "double",
        };

        return kTexts.at(settingValue.index());
    }

    const char* getUserComparatorText(UserComparator comparator) {
        switch (comparator) {
        case UserComparator::TextIsOneOf:
        case UserComparator::SensitiveTextIsOneOf:
        case UserComparator::SemVerIsOneOf: return "IS ONE OF";

        case UserComparator::TextIsNotOneOf:
        case UserComparator::SensitiveTextIsNotOneOf:
        case UserComparator::SemVerIsNotOneOf: return "IS NOT ONE OF";

        case UserComparator::TextContainsAnyOf: return "CONTAINS ANY OF";

        case UserComparator::TextNotContainsAnyOf: return "NOT CONTAINS ANY OF";

        case UserComparator::SemVerLess:
        case UserComparator::NumberLess: return "<";

        case UserComparator::SemVerLessOrEquals:
        case UserComparator::NumberLessOrEquals: return "<=";

        case UserComparator::SemVerGreater:
        case UserComparator::NumberGreater: return ">";

        case UserComparator::SemVerGreaterOrEquals:
        case UserComparator::NumberGreaterOrEquals: return ">=";

        case UserComparator::NumberEquals: return "=";

        case UserComparator::NumberNotEquals: return "!=";

        case UserComparator::DateTimeBefore: return "BEFORE";

        case UserComparator::DateTimeAfter: return "AFTER";

        case UserComparator::TextEquals:
        case UserComparator::SensitiveTextEquals: return "EQUALS";

        case UserComparator::TextNotEquals:
        case UserComparator::SensitiveTextNotEquals: return "NOT EQUALS";

        case UserComparator::TextStartsWithAnyOf:
        case UserComparator::SensitiveTextStartsWithAnyOf: return "STARTS WITH ANY OF";

        case UserComparator::TextNotStartsWithAnyOf:
        case UserComparator::SensitiveTextNotStartsWithAnyOf: return "NOT STARTS WITH ANY OF";

        case UserComparator::TextEndsWithAnyOf:
        case UserComparator::SensitiveTextEndsWithAnyOf: return "ENDS WITH ANY OF";

        case UserComparator::TextNotEndsWithAnyOf:
        case UserComparator::SensitiveTextNotEndsWithAnyOf: return "NOT ENDS WITH ANY OF";

        case UserComparator::ArrayContainsAnyOf:
        case UserComparator::SensitiveArrayContainsAnyOf: return "ARRAY CONTAINS ANY OF";

        case UserComparator::ArrayNotContainsAnyOf:
        case UserComparator::SensitiveArrayNotContainsAnyOf: return "ARRAY NOT CONTAINS ANY OF";

        default: return kInvalidOperatorPlaceholder;
        }
    }

    std::string formatSettingValue(const SettingValue& settingValue) {
        if (const auto textPtr = get_if<string>(&settingValue)) {
            return *textPtr;
        }
        else {
            auto value = static_cast<std::optional<Value>>(settingValue);
            return value ? value->toString() : kInvalidValuePlaceholder;
        }
    }

    std::string formatUserConditionComparisonValue(const UserConditionComparisonValue& comparisonValue) {
        if (const auto textPtr = get_if<string>(&comparisonValue)) {
            return *textPtr;
        }
        else if (const auto numberPtr = get_if<double>(&comparisonValue)) {
            return to_string(*numberPtr);
        }
        else if (const auto stringArrayPtr = get_if<vector<string>>(&comparisonValue)) {
            ostringstream ss;
            ss << "[";
            appendStringList(ss, *stringArrayPtr);
            ss << "]";
            return ss.str();
        }
        else {
            return kInvalidValuePlaceholder;
        }
    }

    std::string formatUserCondition(const UserCondition& condition) {
        EvaluateLogBuilder logBuilder;
        logBuilder.appendUserCondition(condition);
        return logBuilder.toString();
    }

} // namespace configcat
