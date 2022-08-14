#include "configcat/config.h"
#include "configcat/log.h"
#include <nlohmann/json.hpp>

using namespace std;
using json = nlohmann::json;

// nlohmann::json std::shared_ptr serialization

namespace nlohmann {
    template <typename T>
    struct adl_serializer<std::shared_ptr<T>> {
        static void to_json(json& j, const std::shared_ptr<T>& opt) {
            if (opt) {
                j = *opt;
            } else {
                j = nullptr;
            }
        }

        static void from_json(const json& j, std::shared_ptr<T>& opt) {
            if (j.is_null()) {
                opt = nullptr;
            } else {
                opt.reset(new T(j.get<T>()));
            }
        }
    };
}  // namespace nlohmann

namespace configcat {

string valueToString(const ValueType& value) {
    return visit([](auto&& arg){
        using T = decay_t<decltype(arg)>;
        if constexpr (is_same_v<T, string>) {
            return arg;
        } else if constexpr (is_same_v<T, char*>) {
            return string(arg);
        } else if constexpr (is_same_v<T, bool>) {
            return string(arg ? "true" : "false");
        } else if constexpr (is_same_v<T, double>) {
            auto str = to_string(arg);
            // Drop unnecessary '0' characters at the end of the string and keep format 0.0 for zero double
            auto pos = str.find_last_not_of('0');
            if (pos != string::npos && str[pos] == '.') {
                ++pos;
            }
            return str.erase(pos + 1, string::npos);
        } else {
            return to_string(arg);
        }
    }, value);
}

// Config serialization

void parseValue(const json& j, Value& value) {
    auto &jsonValue = j.at(Config::kValue);
    if (jsonValue.is_boolean()) {
        value = jsonValue.get<bool>();
    } else if (jsonValue.is_string()) {
        value = jsonValue.get<std::string>();
    } else if (jsonValue.is_number_integer()) {
        value = jsonValue.get<int>();
    } else if (jsonValue.is_number_unsigned()) {
        value = jsonValue.get<unsigned int>();
    } else if (jsonValue.is_number_float()) {
        value = jsonValue.get<double>();
    } else {
        throw json::parse_error::create(105, 0, string("Invalid value type: ") + jsonValue.type_name(), j);
    }
}

void from_json(const json& j, Preferences& preferences) {
    j.at(Config::kPreferencesUrl).get_to(preferences.url);
    j.at(Config::kPreferencesRedirect).get_to(preferences.redirect);
}

void from_json(const json& j, RolloutPercentageItem& rolloutPercentageItem) {
    parseValue(j, rolloutPercentageItem.value);
    j.at(Config::kPercentage).get_to(rolloutPercentageItem.percentage);
    if (auto it = j.find(Config::kVariationId); it != j.end()) it->get_to(rolloutPercentageItem.variationId);
}

void from_json(const json& j, RolloutRule& rolloutRule) {
    parseValue(j, rolloutRule.value);
    j.at(Config::kComparisonAttribute).get_to(rolloutRule.comparisonAttribute);
    j.at(Config::kComparator).get_to(rolloutRule.comparator);
    j.at(Config::kComparisonValue).get_to(rolloutRule.comparisonValue);
    if (auto it = j.find(Config::kVariationId); it != j.end()) it->get_to(rolloutRule.variationId);
}

void from_json(const json& j, Setting& setting) {
    parseValue(j, setting.value);
    if (auto it = j.find(Config::kRolloutPercentageItems); it != j.end()) it->get_to(setting.percentageItems);
    if (auto it = j.find(Config::kRolloutRules); it != j.end()) it->get_to(setting.rolloutRules);
    if (auto it = j.find(Config::kVariationId); it != j.end()) it->get_to(setting.variationId);
}

void from_json(const json& j, Config& config) {
    if (auto it = j.find(Config::kPreferences); it != j.end()) it->get_to(config.preferences);
    if (auto it = j.find(Config::kEntries); it != j.end()) it->get_to(config.entries);
}

shared_ptr<Config> Config::fromJson(const string& jsonString, const string& eTag) {
    try {
        json ConfigObj = json::parse(jsonString);
        auto config = make_shared<Config>();
        ConfigObj.get_to(*config);
        return config;
    } catch (json::exception& ex) {
        LOG_ERROR << "Config JSON parsing failed. " << ex.what();
        return Config::empty;
    }
}

} // namespace configcat
