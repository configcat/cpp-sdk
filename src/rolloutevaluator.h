#pragma once

#include <string>
#include <tuple>
#include <optional>
#include "configcat/config.h"

class SHA1;
class SHA256;

namespace configcat {

class ConfigCatUser;
class ConfigCatLogger;

struct EvaluateResult {
    const SettingValueContainer& selectedValue;
    const TargetingRule* targetingRule;
    const PercentageOption* percentageOption;
    std::string error;
};

class RolloutEvaluator {
public:
    RolloutEvaluator(const std::shared_ptr<ConfigCatLogger>& logger);
    ~RolloutEvaluator();

    // Evaluate the feature flag or setting
    const EvaluateResult evaluate(const std::string& key,
                                            const std::shared_ptr<ConfigCatUser>& user,
                                            const Setting& setting);

    inline static std::string formatNoMatchRule(const std::string& comparisonAttribute,
                                                const std::string& userValue,
                                                UserComparator comparator,
                                                const UserConditionComparisonValue& comparisonValue);

    inline static std::string formatMatchRule(const std::string& comparisonAttribute,
                                              const std::string& userValue,
                                              UserComparator comparator,
                                              const UserConditionComparisonValue& comparisonValue,
                                              const SettingValueContainer& returnValue);

    inline static std::string formatValidationErrorRule(const std::string& comparisonAttribute,
                                                        const std::string& userValue,
                                                        UserComparator comparator,
                                                        const UserConditionComparisonValue& comparisonValue,
                                                        const std::string& error);

private:
    std::shared_ptr<ConfigCatLogger> logger;
    std::unique_ptr<SHA1> sha1;
    std::unique_ptr<SHA256> sha256;
};

} // namespace configcat
