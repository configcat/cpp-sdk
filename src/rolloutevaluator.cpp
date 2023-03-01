#include "rolloutevaluator.h"
#include "configcat/configcatuser.h"
#include "configcat/configcatlogger.h"
#include "utils.h"
#include <algorithm>
#include <sstream>
#include <hash-library/sha1.h>
#include <semver/semver.hpp>

using namespace std;

namespace configcat {

RolloutEvaluator::RolloutEvaluator(std::shared_ptr<ConfigCatLogger> logger):
    logger(logger),
    sha1(make_unique<SHA1>()) {
}

RolloutEvaluator::~RolloutEvaluator() {
}

template<typename ValueType>
tuple<Value, string, const RolloutRule*, const RolloutPercentageItem*, string> RolloutEvaluator::evaluate(
    const string& key,
    const ConfigCatUser* user,
    const ValueType& defaultValue,
    const string& defaultVariationId,
    const Setting& setting) {
    LogEntry logEntry(logger, LOG_LEVEL_INFO);
    logEntry << "Evaluating getValue(" << key << ")";

    if (!user) {
        if (!setting.rolloutRules.empty() || !setting.percentageItems.empty()) {
            LOG_WARN << "UserObject missing! You should pass a UserObject to getValue(), "
                     << "in order to make targeting work properly. "
                     << "Read more: https://configcat.com/docs/advanced/user-object/";
        }

        logEntry << "\n" << "Returning " << setting.value;
        return {setting.value, setting.variationId, {}, {}, logEntry.getMessage()};
    }

    logEntry << "\n" << "User object: " << user;

    for (const auto& rule : setting.rolloutRules) {
        const auto& comparisonAttribute = rule.comparisonAttribute;
        const auto& comparisonValue = rule.comparisonValue;
        const auto& comparator = rule.comparator;
        const auto* userValuePtr = user->getAttribute(comparisonAttribute);
        const auto& returnValue = rule.value;

        if (userValuePtr == nullptr || comparisonValue.empty() || userValuePtr->empty()) {
            logEntry << "\n" << formatNoMatchRule(comparisonAttribute, userValuePtr ? *userValuePtr : "null", comparator, comparisonValue);
            continue;
        }

        const string& userValue = *userValuePtr;

        switch (comparator) {
            case ONE_OF: {
                stringstream stream(comparisonValue);
                string token;
                while (getline(stream, token, ',')) {
                    trim(token);
                    if (token == userValue) {
                        logEntry << "\n" << formatMatchRule(comparisonAttribute, userValue, comparator, comparisonValue, returnValue);
                        return {rule.value, rule.variationId, &rule, {}, {}};
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
                    if (token == userValue) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    logEntry << "\n" << formatMatchRule(comparisonAttribute, userValue, comparator, comparisonValue, returnValue);
                    return {rule.value, rule.variationId, &rule, {}, {}};
                }
                break;
            }
            case CONTAINS: {
                if (userValue.find(comparisonValue) != std::string::npos) {
                    logEntry << "\n" << formatMatchRule(comparisonAttribute, userValue, comparator, comparisonValue, returnValue);
                    return {rule.value, rule.variationId, &rule, {}, {}};
                }
                break;
            }
            case NOT_CONTAINS: {
                if (userValue.find(comparisonValue) == std::string::npos) {
                    logEntry << "\n" << formatMatchRule(comparisonAttribute, userValue, comparator, comparisonValue, returnValue);
                    return {rule.value, rule.variationId, &rule, {}, {}};
                }
                break;
            }
            case ONE_OF_SEMVER:
            case NOT_ONE_OF_SEMVER: {
                // The rule will be ignored if we found an invalid semantic version
                try {
                    semver::version userValueVersion = semver::version::parse(userValue);
                    stringstream stream(comparisonValue);
                    string token;
                    bool matched = false;
                    while (getline(stream, token, ',')) {
                        trim(token);

                        // Filter empty versions
                        if (token.empty())
                            continue;

                        semver::version tokenVersion = semver::version::parse(token);
                        matched = tokenVersion == userValueVersion || matched;
                    }

                    if ((matched && comparator == ONE_OF_SEMVER) ||
                        (!matched && comparator == NOT_ONE_OF_SEMVER)) {
                        logEntry << "\n" << formatMatchRule(comparisonAttribute, userValue, comparator, comparisonValue,
                                                            returnValue);
                        return {rule.value, rule.variationId, &rule, {}, {}};
                    }
                } catch (semver::semver_exception& exception) {
                    auto message = formatValidationErrorRule(comparisonAttribute, userValue, comparator,
                                                             comparisonValue, exception.what());
                    logEntry << "\n" << message;
                    LOG_WARN << message;
                }
                break;
            }
            case LT_SEMVER:
            case LTE_SEMVER:
            case GT_SEMVER:
            case GTE_SEMVER: {
                // The rule will be ignored if we found an invalid semantic version
                try {
                    string userValueCopy = userValue;
                    semver::version userValueVersion = semver::version::parse(userValueCopy);

                    string comparisonValueCopy = comparisonValue;
                    semver::version comparisonValueVersion = semver::version::parse(trim(comparisonValueCopy));

                    if ((comparator == LT_SEMVER && userValueVersion < comparisonValueVersion) ||
                        (comparator == LTE_SEMVER && userValueVersion <= comparisonValueVersion) ||
                        (comparator == GT_SEMVER && userValueVersion > comparisonValueVersion) ||
                        (comparator == GTE_SEMVER && userValueVersion >= comparisonValueVersion)) {
                        logEntry << "\n" << formatMatchRule(comparisonAttribute, userValue, comparator, comparisonValue,
                                                            returnValue);
                        return {rule.value, rule.variationId, &rule, {}, {}};
                    }
                } catch (semver::semver_exception& exception) {
                    auto message = formatValidationErrorRule(comparisonAttribute, userValue, comparator,
                                                             comparisonValue, exception.what());
                    logEntry << "\n" << message;
                    LOG_WARN << message;
                }
                break;
            }
            case EQ_NUM:
            case NOT_EQ_NUM:
            case LT_NUM:
            case LTE_NUM:
            case GT_NUM:
            case GTE_NUM: {
                bool error = false;
                double userValueDouble = str_to_double(userValue, error);
                if (error) {
                    string reason = string_format("Cannot convert string \"%s\" to double.", userValue.c_str());
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
                    return {rule.value, rule.variationId, &rule, {}, {}};
                }
                break;
            }
            case ONE_OF_SENS: {
                stringstream stream(comparisonValue);
                string token;
                auto userValueHash = (*sha1)(userValue);
                while (getline(stream, token, ',')) {
                    trim(token);
                    if (token == userValueHash) {
                        logEntry << "\n" << formatMatchRule(comparisonAttribute, userValue, comparator, comparisonValue, returnValue);
                        return {rule.value, rule.variationId, &rule, {}, {}};
                    }
                }
                break;
            }
            case NOT_ONE_OF_SENS: {
                stringstream stream(comparisonValue);
                string token;
                auto userValueHash = (*sha1)(userValue);
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
                    return {rule.value, rule.variationId, &rule, {}, {}};
                }
                break;
            }
            default:
                continue;
        }

        logEntry << "\n" << formatNoMatchRule(comparisonAttribute, userValue, comparator, comparisonValue);
    }

    if (!setting.percentageItems.empty()) {
        auto hashCandidate = key + user->identifier;
        string hash = (*sha1)(hashCandidate).substr(0, 7);
        auto num = std::stoul(hash, nullptr, 16);
        auto scaled = num % 100;
        double bucket = 0;
        for (const auto& rolloutPercentageItem : setting.percentageItems) {
            bucket += rolloutPercentageItem.percentage;
            if (scaled < bucket) {
                logEntry << "\n" << "Evaluating %% options. Returning " << rolloutPercentageItem.value;
                return {rolloutPercentageItem.value, rolloutPercentageItem.variationId, {}, &rolloutPercentageItem, {}};
            }
        }
    }

    logEntry << "\n" << "Returning " << setting.value;
    return {setting.value, setting.variationId, {}, {}, {}};
}

std::string RolloutEvaluator::formatNoMatchRule(const std::string& comparisonAttribute,
                                                const std::string& userValue,
                                                Comparator comparator,
                                                const std::string& comparisonValue) {
    return string_format("Evaluating rule: [%s:%s] [%s] [%s] => no match",
                         comparisonAttribute.c_str(),
                         userValue.c_str(),
                         comparatorToString(comparator),
                         comparisonValue.c_str());
}

std::string RolloutEvaluator::formatMatchRule(const std::string& comparisonAttribute,
                                              const std::string& userValue,
                                              Comparator comparator,
                                              const std::string& comparisonValue,
                                              const Value& returnValue) {
    return string_format("Evaluating rule: [%s:%s] [%s] [%s] => match, returning: %s",
                         comparisonAttribute.c_str(),
                         userValue.c_str(),
                         comparatorToString(comparator),
                         comparisonValue.c_str(),
                         valueToString(returnValue).c_str());
}

std::string RolloutEvaluator::formatValidationErrorRule(const std::string& comparisonAttribute,
                                                        const std::string& userValue,
                                                        Comparator comparator,
                                                        const std::string& comparisonValue,
                                                        const std::string& error) {
    return string_format("Evaluating rule: [%s:%s] [%s] [%s] => Skip rule, Validation error: %s",
                         comparisonAttribute.c_str(),
                         userValue.c_str(),
                         comparatorToString(comparator),
                         comparisonValue.c_str(),
                         error.c_str());
}

} // namespace configcat
