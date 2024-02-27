#include <assert.h>
#include <chrono>

#include "configcat/configcatclient.h"
#include "configcat/configcatuser.h"
#include "configcat/log.h"
#include "rolloutevaluator.h"
#include "configservice.h"
#include "configcat/flagoverrides.h"
#include "configcat/overridedatasource.h"
#include "configcat/configcatlogger.h"
#include "configcat/consolelogger.h"
#include "utils.h"

using namespace std;
using namespace std::chrono;

namespace configcat {

std::mutex ConfigCatClient::instancesMutex;
std::unordered_map<std::string, std::unique_ptr<ConfigCatClient>> ConfigCatClient::instances;

ConfigCatClient* ConfigCatClient::get(const std::string& sdkKey, const ConfigCatOptions* options) {
    if (sdkKey.empty()) {
        throw std::invalid_argument("The SDK key cannot be empty.");
    }

    lock_guard<mutex> lock(instancesMutex);
    auto client = instances.find(sdkKey);
    if (client != instances.end()) {
        if (options) {
            LOG_WARN_OBJECT(client->second->logger, 3000) <<
                "There is an existing client instance for the specified SDK Key. "
                "No new client instance will be created and the specified options are ignored. "
                "Returning the existing client instance. SDK Key: '" << sdkKey << "'.";
        }
        return client->second.get();
    }

    client = instances.insert({
        sdkKey,
        std::move(std::unique_ptr<ConfigCatClient>(new ConfigCatClient(sdkKey, options ? *options : ConfigCatOptions())))
    }).first;

    return client->second.get();
}

void ConfigCatClient::close(ConfigCatClient* client) {
    lock_guard<mutex> lock(instancesMutex);
    for (auto it = instances.begin(); it != instances.end(); ++it) {
        if (it->second.get() == client) {
            instances.erase(it);
            return;
        }
    }

    LOG_ERROR_OBJECT(client->logger, 0) << "Client does not exist.";
    assert(false);
}

void ConfigCatClient::closeAll() {
    lock_guard<mutex> lock(instancesMutex);
    instances.clear();
}

size_t ConfigCatClient::instanceCount() {
    return instances.size();
}

ConfigCatClient::ConfigCatClient(const std::string& sdkKey, const ConfigCatOptions& options) {
    hooks = options.hooks ? options.hooks : make_shared<Hooks>();
    logger = make_shared<ConfigCatLogger>(
        options.logger ? options.logger : make_shared<ConsoleLogger>(), hooks
    );

    defaultUser = options.defaultUser;
    rolloutEvaluator = make_unique<RolloutEvaluator>(logger);
    if (options.flagOverrides) {
        overrideDataSource = options.flagOverrides->createDataSource(logger);
    }

    auto configCache = options.configCache ? options.configCache : make_shared<NullConfigCache>();

    if (!overrideDataSource || overrideDataSource->getBehaviour() != LocalOnly) {
        configService = make_unique<ConfigService>(sdkKey, logger, hooks, configCache, options);
    }
}

SettingResult ConfigCatClient::getSettings() const {
    if (overrideDataSource) {
        switch (overrideDataSource->getBehaviour()) {
            case LocalOnly:
                return { overrideDataSource->getOverrides(), kDistantPast };
            case LocalOverRemote: {
                auto settingResult = configService->getSettings();
                auto remote = settingResult.settings;
                auto local = overrideDataSource->getOverrides();
                auto result = make_shared<Settings>();
                if (remote) {
                    for (auto& it : *remote) {
                        result->insert_or_assign(it.first, it.second);
                    }
                }
                if (local) {
                    for (auto& it : *local) {
                        result->insert_or_assign(it.first, it.second);
                    }
                }

                return { result, settingResult.fetchTime };
            }
            case RemoteOverLocal:
                auto settingResult = configService->getSettings();
                auto remote = settingResult.settings;
                auto local = overrideDataSource->getOverrides();
                auto result = make_shared<Settings>();
                if (local) {
                    for (auto& it : *local) {
                        result->insert_or_assign(it.first, it.second);
                    }
                }
                if (remote) {
                    for (auto& it : *remote) {
                        result->insert_or_assign(it.first, it.second);
                    }
                }

                return { result, settingResult.fetchTime };
        }
    }

    return configService->getSettings();
}

bool ConfigCatClient::getValue(const std::string& key, bool defaultValue, const std::shared_ptr<ConfigCatUser>& user) const {
    return _getValue(key, defaultValue, user);
}

int32_t ConfigCatClient::getValue(const std::string& key, int32_t defaultValue, const std::shared_ptr<ConfigCatUser>& user) const {
    return _getValue(key, defaultValue, user);
}

double ConfigCatClient::getValue(const std::string& key, double defaultValue, const std::shared_ptr<ConfigCatUser>& user) const {
    return _getValue(key, defaultValue, user);
}

std::string ConfigCatClient::getValue(const std::string& key, const char* defaultValue, const std::shared_ptr<ConfigCatUser>& user) const {
    return _getValue(key, string(defaultValue), user);
}

std::string ConfigCatClient::getValue(const std::string& key, const std::string& defaultValue, const std::shared_ptr<ConfigCatUser>& user) const {
    return _getValue(key, defaultValue, user);
}

std::optional<Value> ConfigCatClient::getValue(const std::string& key, const std::shared_ptr<ConfigCatUser>& user) const {
    return _getValue<optional<Value>>(key, nullopt, user);
}

EvaluationDetails<bool> ConfigCatClient::getValueDetails(const std::string& key, bool defaultValue, const std::shared_ptr<ConfigCatUser>& user) const {
    return _getValueDetails(key, defaultValue, user);
}

EvaluationDetails<int32_t> ConfigCatClient::getValueDetails(const std::string& key, int32_t defaultValue, const std::shared_ptr<ConfigCatUser>& user) const {
    return _getValueDetails(key, defaultValue, user);
}

EvaluationDetails<double> ConfigCatClient::getValueDetails(const std::string& key, double defaultValue, const std::shared_ptr<ConfigCatUser>& user) const {
    return _getValueDetails(key, defaultValue, user);
}
EvaluationDetails<std::string> ConfigCatClient::getValueDetails(const std::string& key, const std::string& defaultValue, const std::shared_ptr<ConfigCatUser>& user) const {
    return _getValueDetails(key, defaultValue, user);
}

EvaluationDetails<std::string> ConfigCatClient::getValueDetails(const std::string& key, const char* defaultValue, const std::shared_ptr<ConfigCatUser>& user) const {
    return _getValueDetails<string>(key, defaultValue, user);
}


EvaluationDetails<std::optional<Value>> ConfigCatClient::getValueDetails(const std::string& key, const std::shared_ptr<ConfigCatUser>& user) const {
    return _getValueDetails<optional<Value>>(key, nullopt, user);
}

template<typename ValueType>
EvaluationDetails<ValueType> ConfigCatClient::_getValueDetails(const std::string& key, ValueType defaultValue, const std::shared_ptr<ConfigCatUser>& user) const {
    auto settingResult = getSettings();
    auto& settings = settingResult.settings;
    auto& fetchTime = settingResult.fetchTime;
    if (!settings) {
        LogEntry logEntry(logger, LOG_LEVEL_ERROR, 1000);
        if constexpr (is_same_v<ValueType, optional<Value>>) {
            logEntry << "Config JSON is not present when evaluating setting '" << key << "'. Returning std::nullopt.";
        }
        else {
            logEntry << "Config JSON is not present when evaluating setting '" << key << "'. Returning the `defaultValue` parameter that you specified in your application: '" << defaultValue << "'.";
        }
        auto details = EvaluationDetails<ValueType>::fromError(key, defaultValue, logEntry.getMessage());
        hooks->invokeOnFlagEvaluated(details);
        return details;
    }

    auto setting = settings->find(key);
    if (setting == settings->end()) {
        vector<string> keys;
        keys.reserve(settings->size());
        for (auto keyValue : *settings) {
            keys.emplace_back("'" + keyValue.first + "'");
        }
        LogEntry logEntry(logger, LOG_LEVEL_ERROR, 1001);
        if constexpr (is_same_v<ValueType, optional<Value>>) {
            logEntry <<
                "Failed to evaluate setting '" << key << "' (the key was not found in config JSON). "
                "Returning the `defaultValue` parameter that you specified in your application: '" << defaultValue << "'. "
                "Available keys: " << keys << ".";
        }
        else {
            logEntry <<
                "Failed to evaluate setting '" << key << "' (the key was not found in config JSON). "
                "Returning std::nullopt. Available keys: " << keys << ".";
        }
        auto details = EvaluationDetails<ValueType>::fromError(key, defaultValue, logEntry.getMessage());
        hooks->invokeOnFlagEvaluated(details);
        return details;
    }

    return evaluate<ValueType>(key, defaultValue, user, setting->second, fetchTime);
}

std::vector<std::string> ConfigCatClient::getAllKeys() const {
    auto settingResult = getSettings();
    auto& settings = settingResult.settings;
    auto& fetchTime = settingResult.fetchTime;
    if (!settings) {
        LOG_ERROR(1000) << "Config JSON is not present. Returning empty list.";
        return {};
    }

    vector<string> keys;
    keys.reserve(settings->size());
    for (auto keyValue : *settings) {
        keys.emplace_back(keyValue.first);
    }
    return keys;
}

std::shared_ptr<KeyValue> ConfigCatClient::getKeyAndValue(const std::string& variationId) const {
    auto settingResult = getSettings();
    auto& settings = settingResult.settings;
    if (!settings) {
        LOG_ERROR(1000) << "Config JSON is not present. Returning null.";
        return nullptr;
    }

    for (auto& keyValue : *settings) {
        auto& key = keyValue.first;
        auto& setting = keyValue.second;
        if (setting.variationId == variationId) {
            return make_shared<KeyValue>(key, *static_cast<optional<Value>>(setting.value));
        }

        for (auto& targetingRule : setting.targetingRules) {
            auto simpleValue = std::get<SettingValueContainer>(targetingRule.then);
            if (simpleValue.variationId == variationId) {
                return make_shared<KeyValue>(key, *static_cast<optional<Value>>(simpleValue.value));
            }
        }

        for (auto& percentageOption : setting.percentageOptions) {
            if (percentageOption.variationId == variationId) {
                return make_shared<KeyValue>(key, *static_cast<optional<Value>>(percentageOption.value));
            }
        }
    }

    LOG_ERROR(2011) << "Could not find the setting for the specified variation ID: '" << variationId << "'.";
    return nullptr;
}

std::unordered_map<std::string, Value> ConfigCatClient::getAllValues(const std::shared_ptr<ConfigCatUser>& user) const {
    auto settingResult = getSettings();
    auto& settings = settingResult.settings;
    auto& fetchTime = settingResult.fetchTime;
    if (!settings) {
        LOG_ERROR(1000) << "Config JSON is not present. Returning empty map.";
        return {};
    }

    std::unordered_map<std::string, Value> result;
    for (auto keyValue : *settings) {
        auto& key = keyValue.first;
        auto details = evaluate<Value>(key, {}, user, keyValue.second, fetchTime);
        result.insert({key, details.value});
    }

    return result;
}

std::vector<EvaluationDetails<Value>> ConfigCatClient::getAllValueDetails(const std::shared_ptr<ConfigCatUser>& user) const {
    auto settingResult = getSettings();
    auto& settings = settingResult.settings;
    auto& fetchTime = settingResult.fetchTime;
    if (!settings) {
        LOG_ERROR(1000) << "Config JSON is not present. Returning empty list.";
        return {};
    }

    std::vector<EvaluationDetails<Value>> result;
    for (auto keyValue : *settings) {
        auto& key = keyValue.first;
        result.push_back(evaluate<Value>(key, {}, user, keyValue.second, fetchTime));
    }

    return result;
}

template<typename ValueType>
ValueType ConfigCatClient::_getValue(const std::string& key, const ValueType& defaultValue, const std::shared_ptr<ConfigCatUser>& user) const {
    auto settingResult = getSettings();
    auto& settings = settingResult.settings;
    auto& fetchTime = settingResult.fetchTime;
    if (!settings) {
        LogEntry logEntry(logger, LOG_LEVEL_ERROR, 1000);
        if constexpr (is_same_v<ValueType, optional<Value>>) {
            logEntry << "Config JSON is not present when evaluating setting '" << key << "'. Returning std::nullopt.";
        }
        else {
            logEntry << "Config JSON is not present when evaluating setting '" << key << "'. Returning the `defaultValue` parameter that you specified in your application: '" << defaultValue << "'.";
        }
        hooks->invokeOnFlagEvaluated(EvaluationDetails<ValueType>::fromError(key, defaultValue, logEntry.getMessage()));
        return defaultValue;
    }

    auto setting = settings->find(key);
    if (setting == settings->end()) {
        vector<string> keys;
        keys.reserve(settings->size());
        for (auto keyValue : *settings) {
            keys.emplace_back("'" + keyValue.first + "'");
        }
        LogEntry logEntry(logger, LOG_LEVEL_ERROR, 1001);
        if constexpr (is_same_v<ValueType, optional<Value>>) {
            logEntry <<
                "Failed to evaluate setting '" << key << "' (the key was not found in config JSON). "
                "Returning the `defaultValue` parameter that you specified in your application: '" << defaultValue << "'. "
                "Available keys: " << keys << ".";
        }
        else {
            logEntry <<
                "Failed to evaluate setting '" << key << "' (the key was not found in config JSON). "
                "Returning std::nullopt. Available keys: " << keys << ".";
        }
        hooks->invokeOnFlagEvaluated(EvaluationDetails<ValueType>::fromError(key, defaultValue, logEntry.getMessage()));
        return defaultValue;
    }

    EvaluationDetails<ValueType> details = evaluate<ValueType>(key, defaultValue, user, setting->second, fetchTime);
    if (details.isDefaultValue) {
        LOG_ERROR(1002) <<
            "Error occurred in the `getValue` method while evaluating setting '" << key << "'. "
            "Returning the `defaultValue` parameter that you specified in your application: '" << defaultValue << "'.";
    }

    return details.value;
}

template<typename ValueType>
EvaluationDetails<ValueType> ConfigCatClient::evaluate(const std::string& key,
                                                       const ValueType& defaultValue,
                                                       const std::shared_ptr<ConfigCatUser>& user,
                                                       const Setting& setting,
                                                       double fetchTime) const {
    
    const std::shared_ptr<ConfigCatUser>& effectiveUser = user ? user : this->defaultUser;
    auto& evaluateResult = rolloutEvaluator->evaluate(key, effectiveUser, setting);
    auto& error = evaluateResult.error;

    auto& value = const_cast<ValueType&>(defaultValue);
    if constexpr (is_same_v<ValueType, optional<Value>>) {
        // This implicit conversion works because we defined it (see `Value::operator SettingValue()`).
        value = evaluateResult.selectedValue.value;
    }
    else if constexpr (is_same_v<ValueType, Value>) {
        value = *static_cast<optional<Value>>(evaluateResult.selectedValue.value);
    }
    else {
        // TODO: RolloutEvaluator will enforce this
        if (holds_alternative<ValueType>(evaluateResult.selectedValue.value)) {
            value = std::get<ValueType>(evaluateResult.selectedValue.value);
        }
    }

    EvaluationDetails<ValueType> details(key,
                                         value,
                                         evaluateResult.selectedValue.variationId,
                                         time_point<system_clock, duration<double>>(duration<double>(fetchTime)),
                                         effectiveUser,
                                         error.empty() ? false : true,
                                         error,
                                         evaluateResult.targetingRule,
                                         evaluateResult.percentageOption);
    hooks->invokeOnFlagEvaluated(details);
    return details;
}

void ConfigCatClient::forceRefresh() {
    if (configService) {
        configService->refresh();
    }
}

void ConfigCatClient::setOnline() {
    if (configService) {
        configService->setOnline();
    }
    else {
        LOG_WARN(3202) << "Client is configured to use the `LocalOnly` override behavior, thus `setOnline()` has no effect.";
    }
}

void ConfigCatClient::setOffline() {
    if (configService) {
        configService->setOffline();
    }
}

bool ConfigCatClient::isOffline() const {
    if (configService) {
        return configService->isOffline();
    }
    return true;
}

} // namespace configcat

