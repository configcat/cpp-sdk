#pragma once

#include <string>
#include <tuple>
#include "configcat/config.h"

class SHA1;

namespace configcat {

class ConfigCatUser;

class RolloutEvaluator {
public:
    RolloutEvaluator();
    ~RolloutEvaluator();

    std::tuple<Value, std::string> evaluate(const Setting& setting, const std::string& key, const ConfigCatUser* user);

    inline static std::string formatNoMatchRule(const std::string& comparisonAttribute,
                                                const std::string& userValue,
                                                Comparator comparator,
                                                const std::string& comparisonValue);

    inline static std::string formatMatchRule(const std::string& comparisonAttribute,
                                              const std::string& userValue,
                                              Comparator comparator,
                                              const std::string& comparisonValue,
                                              const Value& returnValue);

    inline static std::string formatValidationErrorRule(const std::string& comparisonAttribute,
                                                        const std::string& userValue,
                                                        Comparator comparator,
                                                        const std::string& comparisonValue,
                                                        const std::string& error);

private:
    std::unique_ptr<SHA1> sha1;
};

} // namespace configcat
