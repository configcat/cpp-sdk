#include <fstream>
#include <nlohmann/json.hpp>

#include "configcat/config.h"
#include "utils.h"

using namespace std;
using json = nlohmann::json;

namespace nlohmann {
    // nlohmann::json std::optional serialization
    template <typename T>
    struct adl_serializer<optional<T>> {
        static void to_json(json& j, const optional<T>& opt) {
            if (opt) {
                j = *opt;
            } else {
                j = nullptr;
            }
        }

        static void from_json(const json& j, optional<T>& opt) {
            if (j.is_null()) {
                opt = nullopt;
            } else {
                opt = optional{ j.get<T>() };
            }
        }
    };
} // namespace nlohmann

namespace configcat {

Value::operator SettingValue() const {
    return visit([](auto&& alt) -> SettingValue {
        return alt;
    }, *this);
}

string Value::toString() const {
    return visit([](auto&& alt) -> string {
        using T = decay_t<decltype(alt)>;
        if constexpr (is_same_v<T, bool>) {
            return alt ? "true" : "false";
        } else if constexpr (is_same_v<T, string>) {
            return alt;
        } else if constexpr (is_same_v<T, int32_t>) {
            return string_format("%d", alt);
        } else if constexpr (is_same_v<T, double>) {
            return number_to_string(alt);
        } else {
            static_assert(always_false_v<T>, "Non-exhaustive visitor.");
        }
    }, *this);
}

// Config serialization

#pragma region SettingValue

struct SettingValuePrivate {
    static void setUnsupportedValue(SettingValue& value, const json& j) {
        value = nullopt;
        value.unsupportedValue = shared_ptr<SettingValue::UnsupportedValue>(new SettingValue::UnsupportedValue{ j.type_name(), j.dump() });
    }
};

void to_json(json& j, const SettingValue& value) {
    if (holds_alternative<bool>(value)) {
        j[SettingValue::kBoolean] = get<bool>(value);
    } else if (holds_alternative<string>(value)) {
        j[SettingValue::kString] = get<string>(value);
    } else if (holds_alternative<int32_t>(value)) {
        j[SettingValue::kInt] = get<int32_t>(value);
    } else if (holds_alternative<double>(value)) {
        j[SettingValue::kDouble] = get<double>(value);
    }
}

void from_json(const json& j, SettingValue& value) {
    auto valueFound = false;
    if (auto it = j.find(SettingValue::kBoolean); it != j.end()) {
        value = it->get<bool>();
        valueFound = true;
    }
    if (auto it = j.find(SettingValue::kString); it != j.end()) {
        if (value = it->get<string>(); valueFound) value = nullopt;
        else valueFound = true;
    }
    if (auto it = j.find(SettingValue::kInt); it != j.end()) {
        if (value = it->get<int32_t>(); valueFound) value = nullopt;
        else valueFound = true;
    }
    if (auto it = j.find(SettingValue::kDouble); it != j.end()) {
        if (value = it->get<double>(); valueFound) value = nullopt;
        else valueFound = true;
    }
    if (!valueFound) {
        SettingValuePrivate::setUnsupportedValue(value, j);
    }
}

SettingValue::operator optional<Value>() const {
    return visit([](auto&& alt) -> optional<Value> {
        using T = decay_t<decltype(alt)>;
        if constexpr (is_same_v<T, nullopt_t>) {
            return nullopt;
        } else {
            return alt;
        }
    }, *this);
}

optional<Value> SettingValue::toValueChecked(SettingType type, bool throwIfInvalid) const
{
    return visit([&](auto&& alt) -> optional<Value> {
        using T = decay_t<decltype(alt)>;

        if constexpr (is_same_v<T, bool>) {
            if (type == SettingType::Boolean) return alt;
        } else if constexpr (is_same_v<T, string>) {
            if (type == SettingType::String) return alt;
        } else if constexpr (is_same_v<T, int32_t>) {
            if (type == SettingType::Int) return alt;
        } else if constexpr (is_same_v<T, double>) {
            if (type == SettingType::Double) return alt;
        } else if constexpr (is_same_v<T, nullopt_t>) {
            if (throwIfInvalid) {
                throw runtime_error(unsupportedValue->type == "null"
                    ? "Setting value is null."
                    : string_format("Setting value '%s' is of an unsupported type (%s).", unsupportedValue->value.c_str(), unsupportedValue->type.c_str()));
            }
            return nullopt;
        } else {
            static_assert(always_false_v<T>, "Non-exhaustive visitor.");
        }

        if (throwIfInvalid) {
            throw runtime_error("Setting value is missing or invalid.");
        }

        return nullopt;
    }, *this);
}

#pragma endregion

#pragma region SettingValueContainer

void to_json(json& j, const SettingValueContainer& container) {
    if (!holds_alternative<nullopt_t>(container.value)) j[SettingValueContainer::kValue] = container.value;
    if (container.variationId) j[SettingValueContainer::kVariationId] = container.variationId;
}

void from_json(const json& j, SettingValueContainer& container) {
    if (auto it = j.find(SettingValueContainer::kValue); it != j.end()) it->get_to(container.value);
    if (auto it = j.find(SettingValueContainer::kVariationId); it != j.end()) it->get_to(container.variationId);
}

#pragma endregion

#pragma region PercentageOption

void to_json(json& j, const PercentageOption& percentageOption) {
    j[PercentageOption::kPercentage] = percentageOption.percentage;
    to_json(j, static_cast<const SettingValueContainer&>(percentageOption));
}

void from_json(const json& j, PercentageOption& percentageOption) {
    j.at(PercentageOption::kPercentage).get_to(percentageOption.percentage);
    from_json(j, static_cast<SettingValueContainer&>(percentageOption));
}

#pragma endregion

#pragma region UserCondition

void to_json(json& j, const UserCondition& condition) {
    j[UserCondition::kComparisonAttribute] = condition.comparisonAttribute;
    j[UserCondition::kComparator] = condition.comparator;
    if (holds_alternative<string>(condition.comparisonValue)) {
        j[UserCondition::kStringComparisonValue] = get<string>(condition.comparisonValue);
    } else if (holds_alternative<double>(condition.comparisonValue)) {
        j[UserCondition::kNumberComparisonValue] = get<double>(condition.comparisonValue);
    } else if (holds_alternative<vector<string>>(condition.comparisonValue)) {
        j[UserCondition::kStringListComparisonValue] = get<vector<string>>(condition.comparisonValue);
    }
}

void from_json(const json& j, UserCondition& condition) {
    j.at(UserCondition::kComparisonAttribute).get_to(condition.comparisonAttribute);
    j.at(UserCondition::kComparator).get_to(condition.comparator);
    auto comparisonValueFound = false;
    if (auto it = j.find(UserCondition::kStringComparisonValue); it != j.end()) {
        condition.comparisonValue = it->get<string>();
        comparisonValueFound = true;
    }
    if (auto it = j.find(UserCondition::kNumberComparisonValue); it != j.end()) {
        if (condition.comparisonValue = it->get<double>(); comparisonValueFound) condition.comparisonValue = nullopt;
        else comparisonValueFound = true;
    }
    if (auto it = j.find(UserCondition::kStringListComparisonValue); it != j.end()) {
        if (condition.comparisonValue = it->get<vector<string>>(); comparisonValueFound) condition.comparisonValue = nullopt;
    }
}

#pragma endregion

#pragma region PrerequisiteFlagCondition

void to_json(json& j, const PrerequisiteFlagCondition& condition) {
    j[PrerequisiteFlagCondition::kPrerequisiteFlagKey] = condition.prerequisiteFlagKey;
    j[PrerequisiteFlagCondition::kComparator] = condition.comparator;
    if (!holds_alternative<nullopt_t>(condition.comparisonValue)) j[PrerequisiteFlagCondition::kComparisonValue] = condition.comparisonValue;
}

void from_json(const json& j, PrerequisiteFlagCondition& condition) {
    j.at(PrerequisiteFlagCondition::kPrerequisiteFlagKey).get_to(condition.prerequisiteFlagKey);
    j.at(PrerequisiteFlagCondition::kComparator).get_to(condition.comparator);
    if (auto it = j.find(PrerequisiteFlagCondition::kComparisonValue); it != j.end()) it->get_to(condition.comparisonValue);
}

#pragma endregion

#pragma region SegmentCondition

void to_json(json& j, const SegmentCondition& condition) {
    j[SegmentCondition::kSegmentIndex] = condition.segmentIndex;
    j[SegmentCondition::kComparator] = condition.comparator;
}

void from_json(const json& j, SegmentCondition& condition) {
    j.at(SegmentCondition::kSegmentIndex).get_to(condition.segmentIndex);
    j.at(SegmentCondition::kComparator).get_to(condition.comparator);
}

#pragma endregion

#pragma region ConditionContainer

void to_json(json& j, const ConditionContainer& container) {
    if (holds_alternative<UserCondition>(container.condition)) {
        j[ConditionContainer::kUserCondition] = get<UserCondition>(container.condition);
    } else if (holds_alternative<PrerequisiteFlagCondition>(container.condition)) {
        j[ConditionContainer::kPrerequisiteFlagCondition] = get<PrerequisiteFlagCondition>(container.condition);
    } else if (holds_alternative<SegmentCondition>(container.condition)) {
        j[ConditionContainer::kSegmentCondition] = get<SegmentCondition>(container.condition);
    }
}

void from_json(const json& j, ConditionContainer& container) {
    auto conditionFound = false;
    if (auto it = j.find(ConditionContainer::kUserCondition); it != j.end()) {
        container.condition = it->get<UserCondition>();
        conditionFound = true;
    }
    if (auto it = j.find(ConditionContainer::kPrerequisiteFlagCondition); it != j.end()) {
        if (container.condition = it->get<PrerequisiteFlagCondition>(); conditionFound) container.condition = nullopt;
        else conditionFound = true;
    }
    if (auto it = j.find(ConditionContainer::kSegmentCondition); it != j.end()) {
        if (container.condition = it->get<SegmentCondition>(); conditionFound)  container.condition = nullopt;
    }
}

#pragma endregion

#pragma region TargetingRule

void to_json(json& j, const TargetingRule& targetingRule) {
    if (!targetingRule.conditions.empty()) j[TargetingRule::kConditions] = targetingRule.conditions;
    if (holds_alternative<SettingValueContainer>(targetingRule.then)) {
        j[TargetingRule::kSimpleValue] = get<SettingValueContainer>(targetingRule.then);
    } else {
        j[TargetingRule::kPercentageOptions] = get<vector<PercentageOption>>(targetingRule.then);
    }
}

void from_json(const json& j, TargetingRule& targetingRule) {
    if (auto it = j.find(TargetingRule::kConditions); it != j.end()) it->get_to(targetingRule.conditions);
    auto thenFound = false;
    if (auto it = j.find(TargetingRule::kSimpleValue); it != j.end()) {
        targetingRule.then = it->get<SettingValueContainer>();
        thenFound = true;
    }
    if (auto it = j.find(TargetingRule::kPercentageOptions); it != j.end()) {
        if (targetingRule.then = it->get<vector<PercentageOption>>(); thenFound) targetingRule.then = nullopt;
    }
}

#pragma endregion

#pragma region Segment

void to_json(json& j, const Segment& segment) {
    j[Segment::kName] = segment.name;
    if (!segment.conditions.empty()) j[Segment::kConditions] = segment.conditions;
}

void from_json(const json& j, Segment& segment) {
    j.at(Segment::kName).get_to(segment.name);
    if (auto it = j.find(Segment::kConditions); it != j.end()) it->get_to(segment.conditions);
}

#pragma endregion

#pragma region Setting

void to_json(json& j, const Setting& setting) {
    j[Setting::kType] = setting.type;
    if (setting.percentageOptionsAttribute) j[Setting::kPercentageOptionsAttribute] = setting.percentageOptionsAttribute;
    if (!setting.targetingRules.empty()) j[Setting::kTargetingRules] = setting.targetingRules;
    if (!setting.percentageOptions.empty()) j[Setting::kPercentageOptions] = setting.percentageOptions;
    to_json(j, static_cast<const SettingValueContainer&>(setting));
}

void from_json(const json& j, Setting& setting) {
    j.at(Setting::kType).get_to(setting.type);
    if (auto it = j.find(Setting::kPercentageOptionsAttribute); it != j.end()) it->get_to(setting.percentageOptionsAttribute);
    if (auto it = j.find(Setting::kTargetingRules); it != j.end()) it->get_to(setting.targetingRules);
    if (auto it = j.find(Setting::kPercentageOptions); it != j.end()) it->get_to(setting.percentageOptions);
    from_json(j, static_cast<SettingValueContainer&>(setting));
}

Setting Setting::fromValue(const SettingValue& value) {
    Setting setting;
    setting.type = value.getSettingType();
    setting.value = value;
    return setting;
}

SettingType Setting::getTypeChecked() const {
    if (hasInvalidType()) {
        throw std::runtime_error("Setting type is invalid.");
    }
    return type;
}

#pragma endregion

#pragma region Preference

void to_json(json& j, const Preferences& preferences) {
    j[Preferences::kBaseUrl] = preferences.baseUrl;
    j[Preferences::kRedirectMode] = preferences.redirectMode;
    if (preferences.salt) {
        j[Preferences::kSalt] = *preferences.salt;
    } else {
        j[Preferences::kSalt] = nullptr;
    }
}

void from_json(const json& j, Preferences& preferences) {
    if (auto it = j.find(Preferences::kBaseUrl); it != j.end()) it->get_to(preferences.baseUrl);
    if (auto it = j.find(Preferences::kRedirectMode); it != j.end()) it->get_to(preferences.redirectMode);
    if (auto it = j.find(Preferences::kSalt); it != j.end()) {
        preferences.salt = make_shared<string>();
        it->get_to(*preferences.salt);
    }
}

#pragma endregion

#pragma region Config

void to_json(json& j, const Config& config) {
    j[Config::kPreferences] = config.preferences ? *config.preferences : Preferences();
    if (config.segments && !config.segments->empty()) j[Config::kSegments] = *config.segments;
    if (config.settings && !config.settings->empty()) j[Config::kSettings] = *config.settings;
}

void from_json(const json& j, Config& config) {
    if (auto it = j.find(Config::kPreferences); it != j.end()) it->get_to(config.preferences);
    if (auto it = j.find(Config::kSegments); it != j.end()) it->get_to(*(config.segments = make_shared<Segments>()));
    if (auto it = j.find(Config::kSettings); it != j.end()) it->get_to(*(config.settings = make_shared<Settings>()));
}

const shared_ptr<const Config> Config::empty = make_shared<Config>();

string Config::toJson() {
    return json(*this).dump();
}

shared_ptr<Config> Config::fromJson(const string& jsonString, bool tolerant) {
    json configObj = json::parse(jsonString, nullptr, true, tolerant); // tolerant = ignore comment
    auto config = make_shared<Config>();
    configObj.get_to(*config);
    config->fixupSaltAndSegments();
    return config;
}

shared_ptr<Config> Config::fromFile(const string& filePath, bool tolerant) {
    ifstream file(filePath);
    json data = json::parse(file, nullptr, true, tolerant); // tolerant = ignore comment
    auto config = make_shared<Config>();
    if (auto it = data.find("flags"); it != data.end()) {
        // Simple (key-value) json format
        config->settings = make_shared<Settings>();
        for (auto& [key, value] : it->items()) {
            SettingValue settingValue;
            if (value.is_boolean()) {
                settingValue = value.get<bool>();
            } else if (value.is_string()) {
                settingValue = value.get<string>();
            } else if (value.is_number_integer()) {
                settingValue = value.get<int32_t>();
            } else if (value.is_number()) {
                settingValue = value.get<double>();
            } else {
                SettingValuePrivate::setUnsupportedValue(settingValue, value);
            }
            config->settings->insert({ key, Setting::fromValue(settingValue) });
        }
    } else {
        // Complex (full-featured) json format
        data.get_to(*config);
        config->fixupSaltAndSegments();
    }
    return config;
}

void Config::fixupSaltAndSegments() {
    if (settings && !settings->empty()) {
        auto configJsonSalt = preferences ? (*preferences).salt : nullptr;

        for (auto& [_, setting] : *settings) {
            setting.configJsonSalt = configJsonSalt;
            setting.segments = segments;
        }
    }
}

#pragma endregion

} // namespace configcat
