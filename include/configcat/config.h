#pragma once

#include <string>
#include <variant>
#include <unordered_map>
#include <vector>

namespace configcat {

using Value = std::variant<bool, std::string, int, unsigned int, double>;

struct Preferences {
    std::string url;
    int redirect = 0;
};

struct RolloutPercentageItem {
    // Value served when the rule is selected during evaluation.
    Value value = 0;

    // The rule's percentage value.
    double percentage = 0.0;

    // The rule's variation ID (for analytical purposes).
    std::string variationId;
};

struct RolloutRule {
    // Value served when the rule is selected during evaluation.
    Value value = 0;

    // The user attribute used in the comparison during evaluation.
    std::string comparisonAttribute;

    // The operator used in the comparison.
    //
    // 0  -> 'IS ONE OF',
    // 1  -> 'IS NOT ONE OF',
    // 2  -> 'CONTAINS',
    // 3  -> 'DOES NOT CONTAIN',
    // 4  -> 'IS ONE OF (SemVer)',
    // 5  -> 'IS NOT ONE OF (SemVer)',
    // 6  -> '< (SemVer)',
    // 7  -> '<= (SemVer)',
    // 8  -> '> (SemVer)',
    // 9  -> '>= (SemVer)',
    // 10 -> '= (Number)',
    // 11 -> '<> (Number)',
    // 12 -> '< (Number)',
    // 13 -> '<= (Number)',
    // 14 -> '> (Number)',
    // 15 -> '>= (Number)',
    // 16 -> 'IS ONE OF (Sensitive)',
    // 17 -> 'IS NOT ONE OF (Sensitive)'
    int comparator;

    // The comparison value compared to the given user attribute.
    std::string comparisonValue;

    // The rule's variation ID (for analytical purposes).
    std::string variationId;
};

struct Setting {
    // Value of the feature flag / setting.
    Value value = 0;

    // Collection of percentage rules that belongs to the feature flag / setting.
    std::vector<RolloutPercentageItem> percentageItems;

    // Collection of targeting rules that belongs to the feature flag / setting.
    std::vector<RolloutRule> rolloutRules;

    // Variation ID (for analytical purposes).
    std::string variationId;
};

struct Config {
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

    static constexpr char kETag[] = "e";
    static constexpr char kTimestamp[] = "t";

    std::string jsonString = "{}";
    std::unordered_map<std::string, Preferences> preferences;
    std::unordered_map<std::string, Setting> entries;
    std::string eTag;

    static std::shared_ptr<Config> fromJson(const std::string& json, const std::string& eTag = "");

    static std::shared_ptr<Config> empty;

    Config() {};
    Config(const Config&) = delete; // Disable copy
};

} // namespace configcat
