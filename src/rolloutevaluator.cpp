#include "rolloutevaluator.h"
#include "configcat/configcatuser.h"
#include "configcat/configcatlogger.h"
#include "utils.h"
#include <algorithm>
#include <sstream>
#include <array>
#include <variant>
#include <hash-library/sha1.h>
#include <hash-library/sha256.h>
#include <semver/semver.hpp>
#include <nlohmann/json.hpp>

using namespace std;

namespace configcat {

static constexpr std::array kComparatorTexts{
    "IS ONE OF",
    "IS NOT ONE OF",
    "CONTAINS",
    "DOES NOT CONTAIN",
    "IS ONE OF (SemVer)",
    "IS NOT ONE OF (SemVer)",
    "< (SemVer)",
    "<= (SemVer)",
    "> (SemVer)",
    ">= (SemVer)",
    "= (Number)",
    "<> (Number)",
    "< (Number)",
    "<= (Number)",
    "> (Number)",
    ">= (Number)",
    "IS ONE OF (Sensitive)",
    "IS NOT ONE OF (Sensitive)"
};

static const char* comparatorToString(UserComparator comparator) {
    return kComparatorTexts.at(static_cast<size_t>(comparator));
}

static string comparisonValueToString(const UserConditionComparisonValue& comparisonValue) {
    return visit([](auto&& alt) {
        using T = std::decay_t<decltype(alt)>;
        nlohmann::json j;
        if constexpr (std::is_same_v<T, string> || std::is_same_v<T, double> || std::is_same_v<T, vector<string>>) {
            j = alt;
        }
        else {
            j = "<invalid value>";
        }
        return j.dump();
    }, comparisonValue);
}

RolloutEvaluator::RolloutEvaluator(std::shared_ptr<ConfigCatLogger> logger):
    logger(logger),
    sha1(make_unique<SHA1>()),
    sha256(make_unique<SHA256>()) {
}

RolloutEvaluator::~RolloutEvaluator() {
}

const EvaluateResult RolloutEvaluator::evaluate(
    const string& key,
    const ConfigCatUser* user,
    const Setting& setting) {
    LogEntry logEntry(logger, LOG_LEVEL_INFO, 5000);
    logEntry << "Evaluating getValue(" << key << ")";

    if (!user) {
        if (!setting.targetingRules.empty() || !setting.percentageOptions.empty()) {
            LOG_WARN(3001) <<
                "Cannot evaluate targeting rules and % options for setting '" << key << "' (User Object is missing). "
                "You should pass a User Object to the evaluation methods like `getValue()` in order to make targeting work properly. "
                "Read more: https://configcat.com/docs/advanced/user-object/";
        }

        logEntry << "\n" << "Returning " << setting.value;
        return { setting, nullptr, nullptr, logEntry.getMessage() };
    }

    logEntry << "\n" << "User object: " << user;

    for (const auto& rule : setting.targetingRules) {
        const auto& condition = get<UserCondition>(rule.conditions[0].condition);

        const auto& comparisonAttribute = condition.comparisonAttribute;
        const auto& comparisonValue = condition.comparisonValue;
        const auto& comparator = condition.comparator;
        const auto* userValuePtr = user->getAttribute(comparisonAttribute);
        const auto& returnValue = get<SettingValueContainer>(rule.then);

        if (userValuePtr == nullptr || userValuePtr->empty()) {
            logEntry << "\n" << formatNoMatchRule(comparisonAttribute, userValuePtr ? *userValuePtr : "null", comparator, comparisonValue);
            continue;
        }

        const string& userValue = *userValuePtr;

        switch (comparator) {
            case UserComparator::TextIsOneOf: {
                auto& texts = get<vector<string>>(comparisonValue);
                for (auto& text : texts) {
                    if (text == userValue) {
                        logEntry << "\n" << formatMatchRule(comparisonAttribute, userValue, comparator, comparisonValue, returnValue);
                        return { returnValue, &rule, nullptr, logEntry.getMessage() };
                    }
                }
                break;
            }
            case UserComparator::TextIsNotOneOf: {
                auto& texts = get<vector<string>>(comparisonValue);
                bool found = false;
                for (auto& text : texts) {
                    if (text == userValue) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    logEntry << "\n" << formatMatchRule(comparisonAttribute, userValue, comparator, comparisonValue, returnValue);
                    return { returnValue, &rule, nullptr, logEntry.getMessage() };
                }
                break;
            }
            case UserComparator::TextContainsAnyOf: {
                auto& text = get<vector<string>>(comparisonValue)[0];
                if (userValue.find(text) != std::string::npos) {
                    logEntry << "\n" << formatMatchRule(comparisonAttribute, userValue, comparator, comparisonValue, returnValue);
                    return { returnValue, &rule, nullptr, logEntry.getMessage() };
                }
                break;
            }
            case UserComparator::TextNotContainsAnyOf: {
                auto& text = get<vector<string>>(comparisonValue)[0];
                if (userValue.find(text) == std::string::npos) {
                    logEntry << "\n" << formatMatchRule(comparisonAttribute, userValue, comparator, comparisonValue, returnValue);
                    return { returnValue, &rule, nullptr, logEntry.getMessage() };
                }
                break;
            }
            case UserComparator::SemVerIsOneOf:
            case UserComparator::SemVerIsNotOneOf: {
                auto& texts = get<vector<string>>(comparisonValue);
                // The rule will be ignored if we found an invalid semantic version
                try {
                    semver::version userValueVersion = semver::version::parse(userValue);
                    bool matched = false;
                    for (auto& text : texts) {
                        string version = text;
                        trim(version);

                        // Filter empty versions
                        if (version.empty())
                            continue;

                        semver::version tokenVersion = semver::version::parse(version);
                        matched = tokenVersion == userValueVersion || matched;
                    }

                    if ((matched && comparator == UserComparator::SemVerIsOneOf) ||
                        (!matched && comparator == UserComparator::SemVerIsNotOneOf)) {
                        logEntry << "\n" << formatMatchRule(comparisonAttribute, userValue, comparator, comparisonValue,
                                                            returnValue);
                        return { returnValue, &rule, nullptr, logEntry.getMessage() };
                    }
                } catch (semver::semver_exception& exception) {
                    auto message = formatValidationErrorRule(comparisonAttribute, userValue, comparator,
                                                             comparisonValue, exception.what());
                    logEntry << "\n" << message;
                    LOG_WARN(0) << message;
                }
                break;
            }
            case UserComparator::SemVerLess:
            case UserComparator::SemVerLessOrEquals:
            case UserComparator::SemVerGreater:
            case UserComparator::SemVerGreaterOrEquals: {
                auto& text = get<string>(comparisonValue);

                // The rule will be ignored if we found an invalid semantic version
                try {
                    string userValueCopy = userValue;
                    semver::version userValueVersion = semver::version::parse(userValueCopy);

                    string comparisonValueCopy = text;
                    semver::version comparisonValueVersion = semver::version::parse(trim(comparisonValueCopy));

                    if ((comparator == UserComparator::SemVerLess && userValueVersion < comparisonValueVersion) ||
                        (comparator == UserComparator::SemVerLessOrEquals && userValueVersion <= comparisonValueVersion) ||
                        (comparator == UserComparator::SemVerGreater && userValueVersion > comparisonValueVersion) ||
                        (comparator == UserComparator::SemVerGreaterOrEquals && userValueVersion >= comparisonValueVersion)) {
                        logEntry << "\n" << formatMatchRule(comparisonAttribute, userValue, comparator, comparisonValue,
                                                            returnValue);
                        return { returnValue, &rule, nullptr, logEntry.getMessage() };
                    }
                } catch (semver::semver_exception& exception) {
                    auto message = formatValidationErrorRule(comparisonAttribute, userValue, comparator,
                                                             comparisonValue, exception.what());
                    logEntry << "\n" << message;
                    LOG_WARN(0) << message;
                }
                break;
            }
            case UserComparator::NumberEquals:
            case UserComparator::NumberNotEquals:
            case UserComparator::NumberLess:
            case UserComparator::NumberLessOrEquals:
            case UserComparator::NumberGreater:
            case UserComparator::NumberGreaterOrEquals: {
                auto& number= get<double>(comparisonValue);

                bool error = false;
                double userValueDouble = str_to_double(userValue, error);
                if (error) {
                    string reason = string_format("Cannot convert string \"%s\" to double.", userValue.c_str());
                    auto message = formatValidationErrorRule(comparisonAttribute, userValue, comparator, comparisonValue, reason);
                    logEntry << "\n" << message;
                    LOG_WARN(0) << message;
                    break;
                }

                if (comparator == UserComparator::NumberEquals && userValueDouble == number ||
                    comparator == UserComparator::NumberNotEquals && userValueDouble != number ||
                    comparator == UserComparator::NumberLess && userValueDouble <  number ||
                    comparator == UserComparator::NumberLessOrEquals && userValueDouble <= number ||
                    comparator == UserComparator::NumberGreater && userValueDouble >  number ||
                    comparator == UserComparator::NumberGreaterOrEquals && userValueDouble >= number) {
                    logEntry << "\n" << formatMatchRule(comparisonAttribute, userValue, comparator, comparisonValue, returnValue);
                    return { returnValue, &rule, nullptr, logEntry.getMessage() };
                }
                break;
            }
            case UserComparator::SensitiveTextIsOneOf: {
                auto configJsonSalt = setting.configJsonSalt ? *setting.configJsonSalt : "";
                auto userValueHash = (*sha256)(userValue + configJsonSalt + key);
                auto& texts = get<vector<string>>(comparisonValue);
                for (auto& text : texts) {
                    if (text == userValueHash) {
                        logEntry << "\n" << formatMatchRule(comparisonAttribute, userValue, comparator, comparisonValue, returnValue);
                        return { returnValue, &rule, nullptr, logEntry.getMessage() };
                    }
                }
                break;
            }
            case UserComparator::SensitiveTextIsNotOneOf: {
                auto configJsonSalt = setting.configJsonSalt ? *setting.configJsonSalt : "";
                auto userValueHash = (*sha256)(userValue + configJsonSalt + key);
                auto& texts = get<vector<string>>(comparisonValue);
                bool found = false;
                for (auto& text : texts) {
                    if (text == userValueHash) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    logEntry << "\n" << formatMatchRule(comparisonAttribute, userValue, comparator, comparisonValue, returnValue);
                    return { returnValue, &rule, nullptr, logEntry.getMessage() };
                }
                break;
            }
            default:
                continue;
        }

        logEntry << "\n" << formatNoMatchRule(comparisonAttribute, userValue, comparator, comparisonValue);
    }

    if (!setting.percentageOptions.empty()) {
        auto hashCandidate = key + user->identifier;
        string hash = (*sha1)(hashCandidate).substr(0, 7);
        auto num = std::stoul(hash, nullptr, 16);
        auto scaled = num % 100;
        double bucket = 0;
        for (const auto& rolloutPercentageItem : setting.percentageOptions) {
            bucket += rolloutPercentageItem.percentage;
            if (scaled < bucket) {
                logEntry << "\n" << "Evaluating %% options. Returning " << rolloutPercentageItem.value;
                return { rolloutPercentageItem, nullptr, &rolloutPercentageItem, logEntry.getMessage() };
            }
        }
    }

    logEntry << "\n" << "Returning " << setting.value;
    return { setting, nullptr, nullptr, logEntry.getMessage() };
}

std::string RolloutEvaluator::formatNoMatchRule(const std::string& comparisonAttribute,
    const std::string& userValue,
    UserComparator comparator,
    const UserConditionComparisonValue& comparisonValue) {
    return string_format("Evaluating rule: [%s:%s] [%s] [%s] => no match",
        comparisonAttribute.c_str(),
        userValue.c_str(),
        comparatorToString(comparator),
        comparisonValueToString(comparisonValue).c_str());
}

std::string RolloutEvaluator::formatMatchRule(const std::string& comparisonAttribute,
    const std::string& userValue,
    UserComparator comparator,
    const UserConditionComparisonValue& comparisonValue,
    const SettingValueContainer& returnValue) {

    optional<Value> v = returnValue.value;
    return string_format("Evaluating rule: [%s:%s] [%s] [%s] => match, returning: %s",
        comparisonAttribute.c_str(),
        userValue.c_str(),
        comparatorToString(comparator),
        comparisonValueToString(comparisonValue).c_str(),
        (v ? v->toString() : "<invalid value>").c_str());
}

std::string RolloutEvaluator::formatValidationErrorRule(const std::string& comparisonAttribute,
    const std::string& userValue,
    UserComparator comparator,
    const UserConditionComparisonValue& comparisonValue,
    const std::string& error) {
    return string_format("Evaluating rule: [%s:%s] [%s] [%s] => Skip rule, Validation error: %s",
        comparisonAttribute.c_str(),
        userValue.c_str(),
        comparatorToString(comparator),
        comparisonValueToString(comparisonValue).c_str(),
        error.c_str());
}

} // namespace configcat
