#include "configcat/rolloutevaluator.h"
#include "configcat/configcatuser.h"
#include "configcat/log.h"
#include "configcat/utils.h"
#include <algorithm>
#include <list>
#include <sstream>
#include <stdlib.h>

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
            case ONE_OF: {
                stringstream stream(comparisonValue);
                string token;
                while (getline(stream, token, ',')) {
                    trim(token);
                    if (token == *userValue) {
                        logEntry << "\n" << formatMatchRule(comparisonAttribute, userValue, comparator, comparisonValue, returnValue);
                        return {rule.value, rule.variationId};
                    }
                }
                break;
            }
            case NOT_ONE_OF: {
                stringstream stream(comparisonValue);
                string token;
                bool found = false;
                while (getline(stream, token, ',')) {
                    trim(token);
                    if (token == *userValue) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    logEntry << "\n" << formatMatchRule(comparisonAttribute, userValue, comparator, comparisonValue, returnValue);
                    return {rule.value, rule.variationId};
                }
                break;
            }
            case CONTAINS: {
                if (userValue->find(comparisonValue) != std::string::npos) {
                    logEntry << "\n" << formatMatchRule(comparisonAttribute, userValue, comparator, comparisonValue, returnValue);
                    return {rule.value, rule.variationId};
                }
                break;
            }
            case NOT_CONTAINS: {
                if (userValue->find(comparisonValue) == std::string::npos) {
                    logEntry << "\n" << formatMatchRule(comparisonAttribute, userValue, comparator, comparisonValue, returnValue);
                    return {rule.value, rule.variationId};
                }
                break;
            }
            case ONE_OF_SEMVER:
            case NOT_ONE_OF_SEMVER:
            case LT_SEMVER:
            case LTE_SEMVER:
            case GT_SEMVER:
            case GTE_SEMVER: {
                break;
            }
            case EQ_NUM:
            case NOT_EQ_NUM:
            case LT_NUM:
            case LTE_NUM:
            case GT_NUM:
            case GTE_NUM: {
                // TODO: move these 2 double-conversions into a function
                char* end = nullptr; // error handler for strtod
                double userValueDouble;
                if (userValue->find(",") != std::string::npos) {
                    string userValueCopy = *userValue;
                    replace(userValueCopy.begin(), userValueCopy.end(), ',', '.');
                    userValueDouble = strtod(userValueCopy.c_str(), &end);
                } else {
                    userValueDouble = strtod(userValue->c_str(), &end);
                }
                // Handle number conversion error
                if (end) {
                    string error = string_format("Cannot convert string \"%s\" to double.", userValue->c_str());
                    auto message = formatValidationErrorRule(comparisonAttribute, userValue, comparator, comparisonValue, error);
                    logEntry << "\n" << message;
                    LOG_WARN << message;
                }

                double comparisonValueDouble;
                end = nullptr;
                if (comparisonValue.find(",") != std::string::npos) {
                    string comparisonValueCopy = comparisonValue;
                    replace(comparisonValueCopy.begin(), comparisonValueCopy.end(), ',', '.');
                    comparisonValueDouble = strtod(comparisonValueCopy.c_str(), &end);
                } else {
                    comparisonValueDouble = strtod(comparisonValue.c_str(), &end);
                }
                // Handle number conversion error
                if (end) {
                    string error = string_format("Cannot convert string \"%s\" to double.", comparisonValue.c_str());
                    auto message = formatValidationErrorRule(comparisonAttribute, userValue, comparator, comparisonValue, error);
                    logEntry << "\n" << message;
                    LOG_WARN << message;
                }

                if (comparator == EQ_NUM     && userValueDouble == comparisonValueDouble ||
                    comparator == NOT_EQ_NUM && userValueDouble != comparisonValueDouble ||
                    comparator == LT_NUM     && userValueDouble <  comparisonValueDouble ||
                    comparator == LTE_NUM    && userValueDouble <= comparisonValueDouble ||
                    comparator == GT_NUM     && userValueDouble >  comparisonValueDouble ||
                    comparator == GTE_NUM    && userValueDouble >= comparisonValueDouble) {
                    logEntry << "\n" << formatMatchRule(comparisonAttribute, userValue, comparator, comparisonValue, returnValue);
                    return {rule.value, rule.variationId};
                }
                break;
            }
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

std::string RolloutEvaluator::formatMatchRule(const std::string& comparisonAttribute,
                                              const std::string* userValue,
                                              Comparator comparator,
                                              const std::string& comparisonValue,
                                              const Value& returnValue) {
    return string_format("Evaluating rule: [%s:%s] [%s] [%s] => match, returning: %s",
                         comparisonAttribute.c_str(),
                         userValue,
                         comparatorToString(comparator),
                         comparisonValue.c_str(),
                         valueToString(returnValue).c_str());
}

std::string RolloutEvaluator::formatValidationErrorRule(const std::string& comparisonAttribute,
                                                        const std::string* userValue,
                                                        Comparator comparator,
                                                        const std::string& comparisonValue,
                                                        const std::string& error) {
    return string_format("Evaluating rule: [%s:%s] [%s] [%s] => Skip rule, Validation error: %s",
                         comparisonAttribute.c_str(),
                         userValue,
                         comparatorToString(comparator),
                         comparisonValue.c_str(),
                         error.c_str());
}


} // namespace configcat
