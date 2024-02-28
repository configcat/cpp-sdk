#pragma once

#include <assert.h>
#include <sstream>

#include "configcat/config.h"
#include "configcat/configcatuser.h"
#include "utils.h"

namespace configcat {

    class EvaluateLogBuilder {
        std::ostringstream ss;
        int indentLevel;

    public:
        EvaluateLogBuilder() : indentLevel(0) {}

        inline EvaluateLogBuilder& resetIndent() {
            this->indentLevel = 0;
            return *this;
        }

        inline EvaluateLogBuilder& increaseIndent() {
            this->indentLevel++;
            return *this;
        }

        inline EvaluateLogBuilder& decreaseIndent() {
            assert(this->indentLevel > 0);
            this->indentLevel--;
            return *this;
        }

        EvaluateLogBuilder& newLine();

        template <typename ValueType>
        EvaluateLogBuilder& newLine(const ValueType& value) {
            return newLine().append(value);
        }

        template <typename ValueType>
        EvaluateLogBuilder& append(const ValueType& value) {
            this->ss << value; 
            return *this;
        }

        template<typename... Args>
        inline EvaluateLogBuilder& appendFormat(const std::string& format, Args... args) {
            this->ss << string_format(format, args...);
            return *this;
        }

        EvaluateLogBuilder& appendUserCondition(UserCondition condition);

        inline EvaluateLogBuilder& appendConditionResult(bool result) { return append(result ? "true" : "false"); }
        EvaluateLogBuilder& appendConditionConsequence(bool result);

        EvaluateLogBuilder& appendTargetingRuleConsequence(const TargetingRule& targetingRule, SettingType settingType, const std::variant<bool, std::string>& isMatchOrError, bool newLine);

        inline std::string toString() const { return ss.str(); }
    private:
        EvaluateLogBuilder& appendUserConditionCore(const std::string& comparisonAttribute, UserComparator comparator, const std::optional<std::string>& comparisonValue);
        EvaluateLogBuilder& appendUserConditionString(const std::string& comparisonAttribute, UserComparator comparator, const UserConditionComparisonValue& comparisonValue, bool isSensitive);
        EvaluateLogBuilder& appendUserConditionStringList(const std::string& comparisonAttribute, UserComparator comparator, const UserConditionComparisonValue& comparisonValue, bool isSensitive);
        EvaluateLogBuilder& appendUserConditionNumber(const std::string& comparisonAttribute, UserComparator comparator, const UserConditionComparisonValue& comparisonValue, bool isDateTime);

        EvaluateLogBuilder& appendTargetingRuleThenPart(const TargetingRule& targetingRule, SettingType settingType, bool newLine);
    };

    const char* getSettingTypeText(SettingType settingType);
    const char* getSettingValueTypeText(const SettingValue& settingValue);
    const char* getUserComparatorText(UserComparator comparator);

    std::string formatSettingValue(const SettingValue& settingValue);
    std::string formatUserConditionComparisonValue(const UserConditionComparisonValue& comparisonValue);
    std::string formatUserCondition(const UserCondition& condition);

} // namespace configcat
