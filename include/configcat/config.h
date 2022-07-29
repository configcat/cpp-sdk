#pragma once

#include <string>
#include <variant>
#include <unordered_map>
#include <vector>
#include <memory>

namespace configcat {

using Value = std::variant<bool, std::string, int, unsigned int, double>;

// TODO: Use template here + change Value logging to use this. 
inline std::string valueToString(const Value& value) {
    if (bool const* pval = std::get_if<bool>(&value))
        return std::to_string(*pval);

    if (std::string const* pval = std::get_if<std::string>(&value))
        return *pval;

    if (int const* pval = std::get_if<int>(&value))
        return std::to_string(*pval);

    if (unsigned int const* pval = std::get_if<unsigned int>(&value))
        return std::to_string(*pval);

    if (double const* pval = std::get_if<double>(&value))
        return std::to_string(*pval);

    assert(false);
    return {};
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
    ONE_OF = 0,        // IS ONE OF
    NOT_ONE_OF,        // IS NOT ONE OF
    CONTAINS,          // CONTAINS
    NOT_CONTAINS,      // DOES NOT CONTAIN
    ONE_OF_SEMVER,     // IS ONE OF (SemVer)
    NOT_ONE_OF_SEMVER, // IS NOT ONE OF (SemVer)
    LT_SEMVER,         // < (SemVer)
    LTE_SEMVER,        // <= (SemVer)
    GT_SEMVER,         // > (SemVer)
    GTE_SEMVER,        // >= (SemVer)
    EQ_NUM,            // = (Number)
    NOT_EQ_NUM,        // <> (Number)
    LT_NUM,            // < (Number)
    LTE_NUM,           // <= (Number)
    GT_NUM,            // > (Number)
    GTE_NUM,           // >= (Number)
    ONE_OF_SENS,       // IS ONE OF (Sensitive)
    NOT_ONE_OF_SENS    // IS NOT ONE OF (Sensitive)
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
    Value value = 0;

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
