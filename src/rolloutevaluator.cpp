#include "configcat/rolloutevaluator.h"
#include "configcat/configcatuser.h"
#include "configcat/log.h"
#include "configcat/utils.h"
#include "list"

using namespace std;

namespace configcat {

std::tuple<Value, std::string> RolloutEvaluator::evaluate(const Setting& setting, const string& key, const ConfigCatUser* user) {
    LogEntry logEntry(LOG_LEVEL_INFO);
    logEntry << "Evaluating getValue(" << key << ")";

    if (!user) {
        if (!setting.rolloutRules.empty() || !setting.percentageItems.empty()) {
            LOG_WARN << "UserObject missing! You should pass a UserObject to getValue(), "
                     << "in order to make targeting work properly. "
                     << "Read more: https://configcat.com/docs/advanced/user-object/";
        }

        logEntry << "\n" << "Returning " << setting.value;
        return {setting.value, setting.variationId};
    }

    logEntry << "\n" << "User object: " << user;

    for (const auto& rule : setting.rolloutRules) {
        const auto& comparisonAttribute = rule.comparisonAttribute;
        const auto& comparisonValue = rule.comparisonValue;
        const auto& comparator = rule.comparator;
        const auto* userValue = user->getAttribute(comparisonAttribute);
        const auto& returnValue = rule.value;

        if (userValue == nullptr || comparisonValue.empty() || userValue->empty()) {
            logEntry << "\n" << formatNoMatchRule(comparisonAttribute, userValue, comparator, comparisonValue);
            continue;
        }

        switch (comparator) {
            case ONE_OF:
                break;
            case NOT_ONE_OF:
            case CONTAINS:
            case NOT_CONTAINS:
            case ONE_OF_SEMVER:
            case NOT_ONE_OF_SEMVER:
            case LT_SEMVER:
            case LTE_SEMVER:
            case GT_SEMVER:
            case GTE_SEMVER:
            case EQ_NUM:
            case NOT_EQ_NUM:
            case LT_NUM:
            case LTE_NUM:
            case GT_NUM:
            case GTE_NUM:
            case ONE_OF_SENS:
            case NOT_ONE_OF_SENS:
            default:
                continue;

            logEntry << "\n" << formatNoMatchRule(comparisonAttribute, userValue, comparator, comparisonValue);
        }
    }

    logEntry << "\n" << "Returning " << setting.value;
    return {setting.value, setting.variationId};
}

std::string RolloutEvaluator::formatNoMatchRule(const std::string& comparisonAttribute,
                                                const std::string* userValue,
                                                Comparator comparator,
                                                const std::string& comparisonValue) {
    return string_format("Evaluating rule: [%s:%s] [%s] [%s] => no match",
                         comparisonAttribute.c_str(),
                         userValue,
                         comparatorToString(comparator),
                         comparisonValue.c_str());
}



} // namespace configcat
