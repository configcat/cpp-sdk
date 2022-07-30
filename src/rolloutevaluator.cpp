#include "configcat/rolloutevaluator.h"
#include "configcat/configcatuser.h"
#include "configcat/log.h"
#include "configcat/utils.h"
#include <algorithm>
#include <list>
#include <sstream>
#include <stdlib.h>
#include <hash-library/sha1.h>

using namespace std;

namespace configcat {

RolloutEvaluator::RolloutEvaluator():
    sha1(make_unique<SHA1>()) {
}

RolloutEvaluator::~RolloutEvaluator() {
}

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
                bool error = false;
                double userValueDouble = str_to_double(*userValue, error);
                if (error) {
                    string reason = string_format("Cannot convert string \"%s\" to double.", userValue->c_str());
                    auto message = formatValidationErrorRule(comparisonAttribute, userValue, comparator, comparisonValue, reason);
                    logEntry << "\n" << message;
                    LOG_WARN << message;
                    break;
                }

                double comparisonValueDouble = str_to_double(comparisonValue, error);
                if (error) {
                    string reason = string_format("Cannot convert string \"%s\" to double.", comparisonValue.c_str());
                    auto message = formatValidationErrorRule(comparisonAttribute, userValue, comparator, comparisonValue, reason);
                    logEntry << "\n" << message;
                    LOG_WARN << message;
                    break;
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
            case ONE_OF_SENS: {
                stringstream stream(comparisonValue);
                string token;
                auto userValueHash = (*sha1)(*userValue);
                while (getline(stream, token, ',')) {
                    trim(token);
                    if (token == userValueHash) {
                        logEntry << "\n" << formatMatchRule(comparisonAttribute, userValue, comparator, comparisonValue, returnValue);
                        return {rule.value, rule.variationId};
                    }
                }
                break;
            }
            case NOT_ONE_OF_SENS: {
                stringstream stream(comparisonValue);
                string token;
                auto userValueHash = (*sha1)(*userValue);
                bool found = false;
                while (getline(stream, token, ',')) {
                    trim(token);
                    if (token == userValueHash) {
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
            default:
                continue;

            logEntry << "\n" << formatNoMatchRule(comparisonAttribute, userValue, comparator, comparisonValue);
        }
    }

    if (!setting.percentageItems.empty()) {
        auto hashCandidate = key + user->identifier;
        string hash = (*sha1)(hashCandidate).substr(0, 7);
        auto num = std::stoul(hash, nullptr, 16);
        auto scaled = num % 100;
        double bucket = 0;
        for (const auto& rule : setting.percentageItems) {
            bucket += rule.percentage;
            if (scaled < bucket) {
                logEntry << "\n" << "Evaluating %% options. Returning " << rule.value;
                return {rule.value, rule.variationId};
            }
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
