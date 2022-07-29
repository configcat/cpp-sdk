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

shared_ptr<Config> Config::empty = make_shared<Config>();

// Config serialization

void parseValue(const json& j, Value& value) {
    auto it = j.find(Config::kValue);
    if (it != j.end()) {
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
            LOG_ERROR << "Config JSON parsing failed. Invalid value type: " << jsonValue.type_name();
            assert(false);
        }
    } else {
        LOG_ERROR << "Config JSON parsing failed. Config value key '" << Config::kValue << "' does not exist.";
        assert(false);
    }
}

void from_json(const json& j, Preferences& preferences) {
    if (auto it = j.find(Config::kPreferencesUrl); it != j.end()) it->get_to(preferences.url);
    if (auto it = j.find(Config::kPreferencesRedirect); it != j.end()) it->get_to(preferences.redirect);
}

void from_json(const json& j, RolloutPercentageItem& rolloutPercentageItem) {
    parseValue(j, rolloutPercentageItem.value);
    if (auto it = j.find(Config::kPercentage); it != j.end()) it->get_to(rolloutPercentageItem.percentage);
    if (auto it = j.find(Config::kVariationId); it != j.end()) it->get_to(rolloutPercentageItem.variationId);
}

void from_json(const json& j, RolloutRule& rolloutRule) {
    parseValue(j, rolloutRule.value);
    if (auto it = j.find(Config::kComparisonAttribute); it != j.end()) it->get_to(rolloutRule.comparisonAttribute);
    if (auto it = j.find(Config::kComparator); it != j.end()) it->get_to(rolloutRule.comparator);
    if (auto it = j.find(Config::kComparisonValue); it != j.end()) it->get_to(rolloutRule.comparisonValue);
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
    if (auto it = j.find(Config::kETag); it != j.end()) it->get_to(config.eTag);
}

shared_ptr<Config> Config::fromJson(const string& jsonString, const string& eTag) {
    json ConfigObj = json::parse(jsonString);
    if (!eTag.empty()) ConfigObj[kETag] = eTag;
    auto config = make_shared<Config>();
    ConfigObj.get_to(*config);
    config->jsonString = ConfigObj.dump();

    return config;
}



} // namespace configcat
