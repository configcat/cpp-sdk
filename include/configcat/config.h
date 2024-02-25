#pragma once

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace configcat {

    struct SettingValue;

    struct Value : public std::variant<bool, std::string, int, double> {
    private:
        using _Base = std::variant<bool, std::string, int, double>;
    public:
        Value(const char* v) : _Base(std::string(v)) {}

        // https://stackoverflow.com/a/59372958/8656352
        template<typename T>
        Value(T*) = delete;

        using _Base::_Base;
        using _Base::operator=;

        operator SettingValue() const;

        std::string toString() const;
    };

    enum class RedirectMode : int {
        No = 0,
        Should = 1,
        Force = 2
    };

    /** Setting type. */
    enum class SettingType : int {
        /** On/off type (feature flag). */
        Boolean = 0,
        /** Text type. */
        String = 1,
        /** Whole number type. */
        Int = 2,
        /** Decimal number type. */
        Double = 3,
    };

    /** User Object attribute comparison operator used during the evaluation process. */
    enum class UserComparator {
        /** IS ONE OF (cleartext) - It matches when the comparison attribute is equal to any of the comparison values. */
        TextIsOneOf = 0,
        /** IS NOT ONE OF (cleartext) - It matches when the comparison attribute is not equal to any of the comparison values. */
        TextIsNotOneOf = 1,
        /** CONTAINS ANY OF (cleartext) - It matches when the comparison attribute contains any comparison values as a substring. */
        TextContainsAnyOf = 2,
        /** NOT CONTAINS ANY OF (cleartext) - It matches when the comparison attribute does not contain any comparison values as a substring. */
        TextNotContainsAnyOf = 3,
        /** IS ONE OF (semver) - It matches when the comparison attribute interpreted as a semantic version is equal to any of the comparison values. */
        SemVerIsOneOf = 4,
        /** IS NOT ONE OF (semver) - It matches when the comparison attribute interpreted as a semantic version is not equal to any of the comparison values. */
        SemVerIsNotOneOf = 5,
        /** &lt; (semver) - It matches when the comparison attribute interpreted as a semantic version is less than the comparison value. */
        SemVerLess = 6,
        /** &lt;= (semver) - It matches when the comparison attribute interpreted as a semantic version is less than or equal to the comparison value. */
        SemVerLessOrEquals = 7,
        /** &gt; (semver) - It matches when the comparison attribute interpreted as a semantic version is greater than the comparison value. */
        SemVerGreater = 8,
        /** &gt;= (semver) - It matches when the comparison attribute interpreted as a semantic version is greater than or equal to the comparison value. */
        SemVerGreaterOrEquals = 9,
        /** = (number) - It matches when the comparison attribute interpreted as a decimal number is equal to the comparison value. */
        NumberEquals = 10,
        /** != (number) - It matches when the comparison attribute interpreted as a decimal number is not equal to the comparison value. */
        NumberNotEquals = 11,
        /** &lt; (number) - It matches when the comparison attribute interpreted as a decimal number is less than the comparison value. */
        NumberLess = 12,
        /** &lt;= (number) - It matches when the comparison attribute interpreted as a decimal number is less than or equal to the comparison value. */
        NumberLessOrEquals = 13,
        /** &gt; (number) - It matches when the comparison attribute interpreted as a decimal number is greater than the comparison value. */
        NumberGreater = 14,
        /** &gt;= (number) - It matches when the comparison attribute interpreted as a decimal number is greater than or equal to the comparison value. */
        NumberGreaterOrEquals = 15,
        /** IS ONE OF (hashed) - It matches when the comparison attribute is equal to any of the comparison values (where the comparison is performed using the salted SHA256 hashes of the values). */
        SensitiveTextIsOneOf = 16,
        /** IS NOT ONE OF (hashed) - It matches when the comparison attribute is not equal to any of the comparison values (where the comparison is performed using the salted SHA256 hashes of the values). */
        SensitiveTextIsNotOneOf = 17,
        /** BEFORE (UTC datetime) - It matches when the comparison attribute interpreted as the seconds elapsed since <see href="https://en.wikipedia.org/wiki/Unix_time">Unix Epoch</see> is less than the comparison value. */
        DateTimeBefore = 18,
        /** AFTER (UTC datetime) - It matches when the comparison attribute interpreted as the seconds elapsed since <see href="https://en.wikipedia.org/wiki/Unix_time">Unix Epoch</see> is greater than the comparison value. */
        DateTimeAfter = 19,
        /** EQUALS (hashed) - It matches when the comparison attribute is equal to the comparison value (where the comparison is performed using the salted SHA256 hashes of the values). */
        SensitiveTextEquals = 20,
        /** NOT EQUALS (hashed) - It matches when the comparison attribute is not equal to the comparison value (where the comparison is performed using the salted SHA256 hashes of the values). */
        SensitiveTextNotEquals = 21,
        /** STARTS WITH ANY OF (hashed) - It matches when the comparison attribute starts with any of the comparison values (where the comparison is performed using the salted SHA256 hashes of the values). */
        SensitiveTextStartsWithAnyOf = 22,
        /** NOT STARTS WITH ANY OF (hashed) - It matches when the comparison attribute does not start with any of the comparison values (where the comparison is performed using the salted SHA256 hashes of the values). */
        SensitiveTextNotStartsWithAnyOf = 23,
        /** ENDS WITH ANY OF (hashed) - It matches when the comparison attribute ends with any of the comparison values (where the comparison is performed using the salted SHA256 hashes of the values). */
        SensitiveTextEndsWithAnyOf = 24,
        /** NOT ENDS WITH ANY OF (hashed) - It matches when the comparison attribute does not end with any of the comparison values (where the comparison is performed using the salted SHA256 hashes of the values). */
        SensitiveTextNotEndsWithAnyOf = 25,
        /** ARRAY CONTAINS ANY OF (hashed) - It matches when the comparison attribute interpreted as a comma-separated list contains any of the comparison values (where the comparison is performed using the salted SHA256 hashes of the values). */
        SensitiveArrayContainsAnyOf = 26,
        /** ARRAY NOT CONTAINS ANY OF (hashed) - It matches when the comparison attribute interpreted as a comma-separated list does not contain any of the comparison values (where the comparison is performed using the salted SHA256 hashes of the values). */
        SensitiveArrayNotContainsAnyOf = 27,
        /** EQUALS (cleartext) - It matches when the comparison attribute is equal to the comparison value. */
        TextEquals = 28,
        /** NOT EQUALS (cleartext) - It matches when the comparison attribute is not equal to the comparison value. */
        TextNotEquals = 29,
        /** STARTS WITH ANY OF (cleartext) - It matches when the comparison attribute starts with any of the comparison values. */
        TextStartsWithAnyOf = 30,
        /** NOT STARTS WITH ANY OF (cleartext) - It matches when the comparison attribute does not start with any of the comparison values. */
        TextNotStartsWithAnyOf = 31,
        /** ENDS WITH ANY OF (cleartext) - It matches when the comparison attribute ends with any of the comparison values. */
        TextEndsWithAnyOf = 32,
        /** NOT ENDS WITH ANY OF (cleartext) - It matches when the comparison attribute does not end with any of the comparison values. */
        TextNotEndsWithAnyOf = 33,
        /** ARRAY CONTAINS ANY OF (cleartext) - It matches when the comparison attribute interpreted as a comma-separated list contains any of the comparison values. */
        ArrayContainsAnyOf = 34,
        /** ARRAY NOT CONTAINS ANY OF (cleartext) - It matches when the comparison attribute interpreted as a comma-separated list does not contain any of the comparison values. */
        ArrayNotContainsAnyOf = 35,
    };

    /** Prerequisite flag comparison operator used during the evaluation process. */
    enum class PrerequisiteFlagComparator {
        /** EQUALS - It matches when the evaluated value of the specified prerequisite flag is equal to the comparison value. */
        Equals = 0,
        /** NOT EQUALS - It matches when the evaluated value of the specified prerequisite flag is not equal to the comparison value. */
        NotEquals = 1
    };

    /** Segment comparison operator used during the evaluation process. */
    enum class SegmentComparator {
        /** IS IN SEGMENT - It matches when the conditions of the specified segment are evaluated to true. */
        IsIn = 0,
        /** IS NOT IN SEGMENT - It matches when the conditions of the specified segment are evaluated to false. */
        IsNotIn = 1,
    };

    template<typename... Types>
    class one_of : public std::variant<std::nullopt_t, Types...> {
        using _Base = std::variant<std::nullopt_t, Types...>;
    public:
        one_of() : _Base(std::nullopt) {}
        using _Base::_Base; // inherit std::variant's ctors
        using _Base::operator=; // inherit std::variant's assignment operators
    };

    std::ostream& operator<<(std::ostream& os, const Value& v);

    struct SettingValuePrivate;

    struct SettingValue : public one_of<bool, std::string, int, double> {
        // Not for maintainers: the indices of variant alternatives must correspond to
        // the SettingType enum's member values because we rely on this! (See e.g. Setting::fromValue)
    private:
        using _Base = one_of<bool, std::string, int, double>;
    protected:
        friend struct SettingValuePrivate;

        struct UnsupportedValue {
            std::string type;
            std::string value;
        };

        std::shared_ptr<UnsupportedValue> unsupportedValue;
    public:
        static constexpr char kBoolean[] = "b";
        static constexpr char kString[] = "s";
        static constexpr char kInt[] = "i";
        static constexpr char kDouble[] = "d";

        SettingValue(const char* v) : _Base(std::string(v)) {}

        // https://stackoverflow.com/a/59372958/8656352
        template<typename T>
        SettingValue(T*) = delete;

        using _Base::_Base;
        using _Base::operator=;

        operator std::optional<Value>() const;
        std::optional<Value> toValueChecked(SettingType type, bool throwIfInvalid = true) const;
    };

    struct SettingValueContainer {
        static constexpr char kValue[] = "v";
        static constexpr char kVariationId[] = "i";

        SettingValue value;
        std::optional<std::string> variationId;
    };

    struct PercentageOption : public SettingValueContainer {
        static constexpr char kPercentage[] = "p";

        uint8_t percentage;
    };

    using PercentageOptions = std::vector<PercentageOption>;

    using UserConditionComparisonValue = one_of<std::string, double, std::vector<std::string>>;

    struct UserCondition {
        static constexpr char kComparisonAttribute[] = "a";
        static constexpr char kComparator[] = "c";
        static constexpr char kStringComparisonValue[] = "s";
        static constexpr char kNumberComparisonValue[] = "d";
        static constexpr char kStringListComparisonValue[] = "l";

        std::string comparisonAttribute;
        UserComparator comparator;
        UserConditionComparisonValue comparisonValue;
    };

    using UserConditions = std::vector<UserCondition>;

    struct PrerequisiteFlagCondition {
        static constexpr char kPrerequisiteFlagKey[] = "f";
        static constexpr char kComparator[] = "c";
        static constexpr char kComparisonValue[] = "v";

        std::string prerequisiteFlagKey;
        PrerequisiteFlagComparator comparator;
        SettingValue comparisonValue;
    };

    struct SegmentCondition {
        static constexpr char kSegmentIndex[] = "s";
        static constexpr char kComparator[] = "c";

        int segmentIndex;
        SegmentComparator comparator;
    };

    using Condition = one_of<UserCondition, PrerequisiteFlagCondition, SegmentCondition>;

    struct ConditionContainer {
        static constexpr char kUserCondition[] = "u";
        static constexpr char kPrerequisiteFlagCondition[] = "p";
        static constexpr char kSegmentCondition[] = "s";

        Condition condition;
    };

    using Conditions = std::vector<ConditionContainer>;

    using TargetingRuleThen = one_of<SettingValueContainer, PercentageOptions>;

    struct TargetingRule {
        static constexpr char kConditions[] = "c";
        static constexpr char kSimpleValue[] = "s";
        static constexpr char kPercentageOptions[] = "p";

        Conditions conditions;
        TargetingRuleThen then;
    };

    using TargetingRules = std::vector<TargetingRule>;

    struct Segment {
        static constexpr char kName[] = "n";
        static constexpr char kConditions[] = "r";

        std::string name;
        UserConditions conditions;
    };

    using Segments = std::vector<Segment>;

    struct Config;
    class RolloutEvaluator;

    struct Setting : public SettingValueContainer {
        static constexpr char kType[] = "t";
        static constexpr char kPercentageOptionsAttribute[] = "a";
        static constexpr char kTargetingRules[] = "r";
        static constexpr char kPercentageOptions[] = "p";

        static Setting fromValue(const SettingValue& value);

        SettingType type;
        std::optional<std::string> percentageOptionsAttribute;
        TargetingRules targetingRules;
        PercentageOptions percentageOptions;

        SettingType getTypeChecked();

    protected:
        friend struct Config;
        friend class RolloutEvaluator;
        std::shared_ptr<std::string> configJsonSalt;
        std::shared_ptr<Segments> segments;
    };

    using Settings = std::unordered_map<std::string, Setting>;

    struct Preferences {
        /**
         * The JSON key of the base url from where the config.json is intended to be downloaded.
         */
        static constexpr char kBaseUrl[] = "u";
        /**
         * The JSON key of the redirect mode that should be used in case the data governance mode is wrongly configured.
         */
        static constexpr char kRedirectMode[] = "r";
        /**
         * The JSON key of The salt that, combined with the feature flag key or segment name, is used to hash values for sensitive text comparisons.
         */
        static constexpr char kSalt[] = "s";

        std::optional<std::string> baseUrl;
        RedirectMode redirectMode = RedirectMode::No;
        std::shared_ptr<std::string> salt;

        Preferences() {}

        Preferences(const Preferences& other) : Preferences() { *this = other; }

        Preferences& operator=(const Preferences& other) {
            this->baseUrl = other.baseUrl;
            this->redirectMode = other.redirectMode;
            this->salt = other.salt ? std::make_shared<std::string>(*other.salt) : std::shared_ptr<std::string>();
            return *this;
        }

        Preferences(Preferences&& other) = default;

        Preferences& operator=(Preferences&& other) = default;
    };

    /**
     * Details of a ConfigCat config.
     */
    struct Config {
        /**
         * The JSON key of preferences of the config.json, mostly for controlling the redirection behaviour of the SDK.
         */
        static constexpr char kPreferences[] = "p";
        /**
         * The JSON key of segment definitions for re-using segment rules in targeting rules.
         */
        static constexpr char kSegments[] = "s";
        /**
         * The JSON key of setting definitions.
         */
        static constexpr char kSettings[] = "f";

        static const std::shared_ptr<const Config> empty;

        std::string toJson();
        static std::shared_ptr<Config> fromJson(const std::string& jsonString, bool tolerant = false);
        static std::shared_ptr<Config> fromFile(const std::string& filePath, bool tolerant = true);

        std::optional<Preferences> preferences;
        std::shared_ptr<Segments> segments;
        std::shared_ptr<Settings> settings;

        std::shared_ptr<Segments> ensureSegments() const { return segments ? segments : std::make_shared<Segments>(); }
        std::shared_ptr<Settings> ensureSettings() const { return settings ? settings : std::make_shared<Settings>(); }

        Config() : segments(std::make_shared<Segments>()), settings(std::make_shared<Settings>()) {}
        
        Config(const Config& other) : Config() { *this = other; }

        Config& operator=(const Config& other) {
            this->preferences = other.preferences;
            this->segments = other.segments ? std::make_shared<Segments>(*other.segments) : nullptr;
            this->settings = other.settings ? std::make_shared<Settings>(*other.settings) : nullptr;
            this->fixup();
            return *this;
        }

        Config(Config&& other) = default;

        Config& operator=(Config&& other) = default;
    private:
        void fixup();
    };

} // namespace configcat
