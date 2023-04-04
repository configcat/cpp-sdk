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

bool ConfigCatClient::getValue(const std::string& key, bool defaultValue, const ConfigCatUser* user) const {
    return _getValue(key, defaultValue, user);
}

int ConfigCatClient::getValue(const std::string& key, int defaultValue, const ConfigCatUser* user) const {
    return _getValue(key, defaultValue, user);
}

double ConfigCatClient::getValue(const std::string& key, double defaultValue, const ConfigCatUser* user) const {
    return _getValue(key, defaultValue, user);
}

std::string ConfigCatClient::getValue(const std::string& key, const char* defaultValue, const ConfigCatUser* user) const {
    return _getValue(key, string(defaultValue), user);
}

std::string ConfigCatClient::getValue(const std::string& key, const std::string& defaultValue, const ConfigCatUser* user) const {
    return _getValue(key, defaultValue, user);
}

std::shared_ptr<Value> ConfigCatClient::getValue(const std::string& key, const ConfigCatUser* user) const {
    auto settingResult = getSettings();
    auto& settings = settingResult.settings;
    auto& fetchTime = settingResult.fetchTime;
    if (!settings || settings->empty()) {
        LogEntry logEntry(logger, LOG_LEVEL_ERROR, 1000);
        logEntry << "Config JSON is not present. Returning nullptr.";
        hooks->invokeOnFlagEvaluated(EvaluationDetails::fromError(key, {}, logEntry.getMessage()));
        return {};
    }

    auto setting = settings->find(key);
    if (setting == settings->end()) {
        vector<string> keys;
        keys.reserve(settings->size());
        for (auto keyValue : *settings) {
            keys.emplace_back("'" + keyValue.first + "'");
        }
        LOG_ERROR(1001) <<
            "Failed to evaluate setting '" << key << "' (the key was not found in config JSON). "
            "Returning nullptr. Available keys: " << keys << ".";
        return {};
    }

    auto details = evaluate(key, user, setting->second, fetchTime);
    return make_shared<Value>(details.value);
}

EvaluationDetails ConfigCatClient::getValueDetails(const std::string& key, bool defaultValue, const ConfigCatUser* user) const {
    return _getValueDetails(key, defaultValue, user);
}

EvaluationDetails ConfigCatClient::getValueDetails(const std::string& key, int defaultValue, const ConfigCatUser* user) const {
    return _getValueDetails(key, defaultValue, user);
}

EvaluationDetails ConfigCatClient::getValueDetails(const std::string& key, double defaultValue, const ConfigCatUser* user) const {
    return _getValueDetails(key, defaultValue, user);
}
EvaluationDetails ConfigCatClient::getValueDetails(const std::string& key, const std::string& defaultValue, const ConfigCatUser* user) const {
    return _getValueDetails(key, defaultValue, user);
}

EvaluationDetails ConfigCatClient::getValueDetails(const std::string& key, const char* defaultValue, const ConfigCatUser* user) const {
    return _getValueDetails(key, defaultValue, user);
}

template<typename ValueType>
EvaluationDetails ConfigCatClient::_getValueDetails(const std::string& key, ValueType defaultValue, const ConfigCatUser* user) const {
    auto settingResult = getSettings();
    auto& settings = settingResult.settings;
    auto& fetchTime = settingResult.fetchTime;
    if (!settings || settings->empty()) {
        LogEntry logEntry(logger, LOG_LEVEL_ERROR, 1000);
        logEntry << "Config JSON is not present. Returning the `defaultValue` parameter that you specified in your application: '" << defaultValue << "'.";
        auto details = EvaluationDetails::fromError(key, defaultValue, logEntry.getMessage());
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
        logEntry <<
            "Failed to evaluate setting '" << key << "' (the key was not found in config JSON). "
            "Returning the `defaultValue` parameter that you specified in your application: '" << defaultValue << "'. "
            "Available keys: " << keys << ".";
        auto details = EvaluationDetails::fromError(key, defaultValue, logEntry.getMessage());
        hooks->invokeOnFlagEvaluated(details);
        return details;
    }

    return evaluate(key, user, setting->second, fetchTime);
}

std::vector<std::string> ConfigCatClient::getAllKeys() const {
    auto settingResult = getSettings();
    auto& settings = settingResult.settings;
    auto& fetchTime = settingResult.fetchTime;
    if (!settings || settings->empty()) {
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
    auto& fetchTime = settingResult.fetchTime;
    if (!settings || settings->empty()) {
        LOG_ERROR(1000) << "Config JSON is not present. Returning null.";
        return nullptr;
    }

    for (auto keyValue : *settings) {
        auto& key = keyValue.first;
        auto setting = keyValue.second;
        if (setting.variationId == variationId) {
            return make_shared<KeyValue>(key, setting.value);
        }

        for (auto rolloutRule : setting.rolloutRules) {
            if (rolloutRule.variationId == variationId) {
                return make_shared<KeyValue>(key, rolloutRule.value);
            }
        }

        for (auto percentageRule : setting.percentageItems) {
            if (percentageRule.variationId == variationId) {
                return make_shared<KeyValue>(key, percentageRule.value);
            }
        }
    }

    return nullptr;
}

std::unordered_map<std::string, Value> ConfigCatClient::getAllValues(const ConfigCatUser* user) const {
    auto settingResult = getSettings();
    auto& settings = settingResult.settings;
    auto& fetchTime = settingResult.fetchTime;
    if (!settings || settings->empty()) {
        LOG_ERROR(1000) << "Config JSON is not present. Returning empty map.";
        return {};
    }

    std::unordered_map<std::string, Value> result;
    for (auto keyValue : *settings) {
        auto& key = keyValue.first;
        auto details = evaluate(key, user, keyValue.second, fetchTime);
        result.insert({key, details.value});
    }

    return result;
}

std::vector<EvaluationDetails> ConfigCatClient::getAllValueDetails(const ConfigCatUser* user) const {
    auto settingResult = getSettings();
    auto& settings = settingResult.settings;
    auto& fetchTime = settingResult.fetchTime;
    if (!settings || settings->empty()) {
        LOG_ERROR(1000) << "Config JSON is not present. Returning empty list.";
        return {};
    }

    std::vector<EvaluationDetails> result;
    for (auto keyValue : *settings) {
        auto& key = keyValue.first;
        result.push_back(evaluate(key, user, keyValue.second, fetchTime));
    }

    return result;
}

template<typename ValueType>
ValueType ConfigCatClient::_getValue(const std::string& key, const ValueType& defaultValue, const ConfigCatUser* user) const {
    auto settingResult = getSettings();
    auto& settings = settingResult.settings;
    auto& fetchTime = settingResult.fetchTime;
    if (!settings || settings->empty()) {
        LogEntry logEntry(logger, LOG_LEVEL_ERROR, 1000);
        logEntry << "Config JSON is not present. Returning the `defaultValue` parameter that you specified in your application: '" << defaultValue << "'.";
        hooks->invokeOnFlagEvaluated(EvaluationDetails::fromError(key, defaultValue, logEntry.getMessage()));
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
        logEntry <<
            "Failed to evaluate setting '" << key << "' (the key was not found in config JSON). "
            "Returning the `defaultValue` parameter that you specified in your application: '" << defaultValue << "'. "
            "Available keys: " << keys << ".";
        hooks->invokeOnFlagEvaluated(EvaluationDetails::fromError(key, defaultValue, logEntry.getMessage()));
        return defaultValue;
    }

    auto details = evaluate(key, user, setting->second, fetchTime);
    const ValueType* valuePtr = get_if<ValueType>(&details.value);
    if (valuePtr)
        return *valuePtr;

    LOG_ERROR(1002) <<
        "Error occurred in the `getValue` method while evaluating setting '" << key << "'. "
        "Returning the `defaultValue` parameter that you specified in your application: '" << defaultValue << "'.";
    return defaultValue;
}

EvaluationDetails ConfigCatClient::evaluate(const std::string& key,
                                            const ConfigCatUser* user,
                                            const Setting& setting,
                                            double fetchTime) const {
    user = user != nullptr ? user : defaultUser.get();
    auto [value, variationId, rule, percentageRule, error] = rolloutEvaluator->evaluate(key, user, setting);

    EvaluationDetails details(key,
                              value,
                              variationId,
                              time_point<system_clock, duration<double>>(duration<double>(fetchTime)),
                              user,
                              error.empty() ? false : true,
                              error,
                              rule,
                              percentageRule);
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

bool ConfigCatClient::isOffline() {
    if (configService) {
        return configService->isOffline();
    }
    return true;
}

} // namespace configcat

