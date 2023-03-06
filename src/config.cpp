#include "configcat/config.h"
#include <nlohmann/json.hpp>
#include <fstream>

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

void to_json(json& j, const Value& value) {
    std::visit([&](auto&& arg){j = arg;}, value);
}

void from_json(const json& j, Value& value) {
    if (j.is_boolean()) {
        value = j.get<bool>();
    } else if (j.is_string()) {
        value = j.get<std::string>();
    } else if (j.is_number_integer()) {
        value = j.get<int>();
    } else if (j.is_number_float()) {
        value = j.get<double>();
    } else {
        throw json::parse_error::create(105, 0, string("Invalid value type: ") + j.type_name(), &j);
    }
}

void to_json(json& j, const Preferences& preferences) {
    j[Config::kPreferencesUrl] = preferences.url;
    j[Config::kPreferencesRedirect] = preferences.redirect;
}

void from_json(const json& j, Preferences& preferences) {
    j.at(Config::kPreferencesUrl).get_to(preferences.url);
    j.at(Config::kPreferencesRedirect).get_to(preferences.redirect);
}

void to_json(json& j, const RolloutPercentageItem& rolloutPercentageItem) {
    j[Config::kValue] = rolloutPercentageItem.value;
    j[Config::kPercentage] = rolloutPercentageItem.percentage;
    if (!rolloutPercentageItem.variationId.empty()) j[Config::kVariationId] = rolloutPercentageItem.variationId;
}

void from_json(const json& j, RolloutPercentageItem& rolloutPercentageItem) {
    j.at(Config::kValue).get_to(rolloutPercentageItem.value);
    j.at(Config::kPercentage).get_to(rolloutPercentageItem.percentage);
    if (auto it = j.find(Config::kVariationId); it != j.end()) it->get_to(rolloutPercentageItem.variationId);
}

void to_json(json& j, const RolloutRule& rolloutRule) {
    j[Config::kValue] = rolloutRule.value;
    j[Config::kComparisonAttribute] = rolloutRule.comparisonAttribute;
    j[Config::kComparator] = rolloutRule.comparator;
    j[Config::kComparisonValue] = rolloutRule.comparisonValue;
    if (!rolloutRule.variationId.empty()) j[Config::kVariationId] = rolloutRule.variationId;
}

void from_json(const json& j, RolloutRule& rolloutRule) {
    j.at(Config::kValue).get_to(rolloutRule.value);
    j.at(Config::kComparisonAttribute).get_to(rolloutRule.comparisonAttribute);
    j.at(Config::kComparator).get_to(rolloutRule.comparator);
    j.at(Config::kComparisonValue).get_to(rolloutRule.comparisonValue);
    if (auto it = j.find(Config::kVariationId); it != j.end()) it->get_to(rolloutRule.variationId);
}

void to_json(json& j, const Setting& setting) {
    j[Config::kValue] = setting.value;
    if (!setting.percentageItems.empty()) j[Config::kRolloutPercentageItems] = setting.percentageItems;
    if (!setting.rolloutRules.empty()) j[Config::kRolloutRules] = setting.rolloutRules;
    if (!setting.variationId.empty()) j[Config::kVariationId] = setting.variationId;
}

void from_json(const json& j, Setting& setting) {
    j.at(Config::kValue).get_to(setting.value);
    if (auto it = j.find(Config::kRolloutPercentageItems); it != j.end()) it->get_to(setting.percentageItems);
    if (auto it = j.find(Config::kRolloutRules); it != j.end()) it->get_to(setting.rolloutRules);
    if (auto it = j.find(Config::kVariationId); it != j.end()) it->get_to(setting.variationId);
}

void to_json(json& j, const Config& value) {
    if (value.preferences) j[Config::kPreferences] = value.preferences;
    if (value.entries) j[Config::kEntries] = value.entries;
}

void from_json(const json& j, Config& config) {
    if (auto it = j.find(Config::kPreferences); it != j.end()) it->get_to(config.preferences);
    if (auto it = j.find(Config::kEntries); it != j.end()) it->get_to(config.entries);
}

std::string Config::toJson() {
    return json(*this).dump();
}

shared_ptr<Config> Config::fromJson(const string& jsonString) {
    json configObj = json::parse(jsonString);
    auto config = make_shared<Config>();
    configObj.get_to(*config);
    return config;
}

shared_ptr<Config> Config::fromFile(const string& filePath) {
    ifstream file(filePath);
    json data = json::parse(file);
    auto config = make_shared<Config>();
    if (auto it = data.find("flags"); it != data.end()) {
        // Simple (key-value) json format
        config->entries = make_shared<unordered_map<string, Setting>>();
        for (auto& [key, value] : it->items()) {
            Setting setting;
            value.get_to(setting.value);
            config->entries->insert({key, setting});
        }
    } else {
        // Complex (full-featured) json format
        data.get_to(*config);
    }
    return config;
}

shared_ptr<ConfigEntry> ConfigEntry::fromJson(const std::string& jsonString) {
    json configEntryObj = json::parse(jsonString);
    auto config = make_shared<Config>();
    auto configObj = configEntryObj.at(kConfig);
    configObj.get_to(*config);
    return make_shared<ConfigEntry>(config, configEntryObj.value(kETag, ""), configEntryObj.value(kFetchTime, kDistantPast));
}

string ConfigEntry::toJson() const {
    return "{"s +
       '"' + kConfig + '"' + ":" + (config ? config->toJson() : "{}") +
       "," + '"' + kETag + '"' + ":" + '"' + eTag + '"' +
       "," + '"' + kFetchTime + '"' + ":" + to_string(fetchTime) +
   "}";
}

} // namespace configcat
