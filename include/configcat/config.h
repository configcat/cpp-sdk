#pragma once

#include <string>
#include <variant>
#include <unordered_map>
#include <vector>
#include <memory>
#include <sstream>

namespace configcat {

using Value = std::variant<bool, std::string, int, unsigned int, double>;

inline std::string valueToString(const Value& value) {
    return std::visit([](auto&& arg){
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, std::string>) {
            return arg;
        } else if constexpr (std::is_same_v<T, bool>) {
            return std::string(arg ? "true" : "false");
        } else if constexpr (std::is_same_v<T, double>) {
            auto str = std::to_string(arg);
            // Drop unnecessary '0' characters at the end of the string and keep format 0.0 for zero double
            auto pos = str.find_last_not_of('0');
            if (pos != std::string::npos && str[pos] == '.') {
                ++pos;
            }
            return str.erase(pos + 1, std::string::npos);
        } else {
            return std::to_string(arg);
        }
    }, value);
}

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

enum Comparator: int {
    ONE_OF = 0,             // IS ONE OF
    NOT_ONE_OF = 1,         // IS NOT ONE OF
    CONTAINS = 2,           // CONTAINS
    NOT_CONTAINS = 3,       // DOES NOT CONTAIN
    ONE_OF_SEMVER = 4,      // IS ONE OF (SemVer)
    NOT_ONE_OF_SEMVER = 5,  // IS NOT ONE OF (SemVer)
    LT_SEMVER = 6,          // < (SemVer)
    LTE_SEMVER = 7,         // <= (SemVer)
    GT_SEMVER = 8,          // > (SemVer)
    GTE_SEMVER = 9,         // >= (SemVer)
    EQ_NUM = 10,            // = (Number)
    NOT_EQ_NUM = 11,        // <> (Number)
    LT_NUM = 12,            // < (Number)
    LTE_NUM = 13,           // <= (Number)
    GT_NUM = 14,            // > (Number)
    GTE_NUM = 15,           // >= (Number)
    ONE_OF_SENS = 16,       // IS ONE OF (Sensitive)
    NOT_ONE_OF_SENS = 17    // IS NOT ONE OF (Sensitive)
};

static constexpr char const* kComparatorTexts[] = {
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

inline const char* comparatorToString(Comparator comparator) {
    return kComparatorTexts[comparator];
}

struct RolloutRule {
    // Value served when the rule is selected during evaluation.
    Value value = 0; // optional

    // The user attribute used in the comparison during evaluation.
    std::string comparisonAttribute;

    // The operator used in the comparison.
    Comparator comparator = ONE_OF;

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
    std::shared_ptr<std::unordered_map<std::string, Preferences>> preferences;
    std::shared_ptr<std::unordered_map<std::string, Setting>> entries;
    std::string eTag;

    static std::shared_ptr<Config> fromJson(const std::string& jsonString, const std::string& eTag = "");

    static std::shared_ptr<Config> empty;

    Config() {};
    Config(const Config&) = delete; // Disable copy
};

} // namespace configcat
