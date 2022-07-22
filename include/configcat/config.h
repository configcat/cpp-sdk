#pragma once

#include <string>
#include <variant>
#include <unordered_map>

namespace configcat {

using Value = std::variant<bool, std::string, int, double>;

class Config {
public:
    static constexpr char kValue[] = "v";
    static constexpr char kComparator[] = "t";
    static constexpr char kComparisonAttribute[] = "a";
    static constexpr char kComparisonValue[] = "c";
    static constexpr char kRolloutPercentageItems[] = "p";
    static constexpr char kPercentage[] = "p";
    static constexpr char kRolloutRules[] = "r";
    static constexpr char kVariationId[] = "i";
    static constexpr char kPreferences[] = "p";
    static constexpr char kPreferencesUrl[] = "u";
    static constexpr char kPreferencesRedirect[] = "r";
    static constexpr char kEntries[] = "f";

    const std::string jsonString;
    std::unordered_map<std::string, Value> preferences;
    std::unordered_map<std::string, Value> entries;

    Config(const std::string& jsonString = "{}",
           std::unordered_map<std::string, Value> preferences = {},
           std::unordered_map<std::string, Value> entries = {}):
        jsonString(jsonString),
        preferences(preferences),
        entries(entries) {
    }

//    static Config empty;
};

} // namespace configcat
