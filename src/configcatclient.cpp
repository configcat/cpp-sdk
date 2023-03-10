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
            LOG_WARN_OBJECT(client->second->logger) << "Client for sdk_key `" << sdkKey
                << "` is already created and will be reused; options passed are being ignored.";
        }
        return client->second.get();
    }

    client = instances.insert({
        sdkKey,
        move(std::unique_ptr<ConfigCatClient>(new ConfigCatClient(sdkKey, options ? *options : ConfigCatOptions())))
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

    LOG_ERROR_OBJECT(client->logger) << "Client does not exist.";
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
        LogEntry logEntry(logger, LOG_LEVEL_ERROR);
        logEntry << "Evaluating getValue(\"" << key << "\") failed. Cache is empty. "
                 << "Returning nullptr.";
        hooks->invokeOnFlagEvaluated(EvaluationDetails::fromError(key, {}, logEntry.getMessage()));
        return {};
    }

    auto setting = settings->find(key);
    if (setting == settings->end()) {
        vector<string> keys;
        keys.reserve(settings->size());
        for (auto keyValue : *settings) {
            keys.emplace_back(keyValue.first);
        }
        LOG_ERROR << "Value not found for key " << key << ". Here are the available keys: " << keys
                  << " Returning nullptr.";
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
        LogEntry logEntry(logger, LOG_LEVEL_ERROR);
        logEntry << "Evaluating getValueDetails(\"" << key << "\") failed. Cache is empty. "
                 << "Returning defaultValue: " << defaultValue << " .";
        auto details = EvaluationDetails::fromError(key, defaultValue, logEntry.getMessage());
        hooks->invokeOnFlagEvaluated(details);
        return details;
    }

    auto setting = settings->find(key);
    if (setting == settings->end()) {
        vector<string> keys;
        keys.reserve(settings->size());
        for (auto keyValue : *settings) {
            keys.emplace_back(keyValue.first);
        }
        LogEntry logEntry(logger, LOG_LEVEL_ERROR);
        logEntry << "Value not found for key " << key << ". Here are the available keys: " << keys
                 << " Returning defaultValue: " << defaultValue << " .";
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
        LOG_ERROR << "Config JSON is not present. Returning null.";
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
        LOG_ERROR << "Evaluating getAllValues() failed. Cache is empty. Returning empty list.";
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
        LOG_ERROR << "Evaluating getAllValueDetails() failed. Cache is empty. Returning empty list.";
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
        LogEntry logEntry(logger, LOG_LEVEL_ERROR);
        logEntry << "Evaluating getValue(\"" << key << "\") failed. Cache is empty. "
                 << "Returning defaultValue: " << defaultValue << " .";
        hooks->invokeOnFlagEvaluated(EvaluationDetails::fromError(key, defaultValue, logEntry.getMessage()));
        return defaultValue;
    }

    auto setting = settings->find(key);
    if (setting == settings->end()) {
        vector<string> keys;
        keys.reserve(settings->size());
        for (auto keyValue : *settings) {
            keys.emplace_back(keyValue.first);
        }
        LogEntry logEntry(logger, LOG_LEVEL_ERROR);
        logEntry << "Value not found for key " << key << ". Here are the available keys: " << keys
                 << " Returning defaultValue: " << defaultValue << " .";
        hooks->invokeOnFlagEvaluated(EvaluationDetails::fromError(key, defaultValue, logEntry.getMessage()));
        return defaultValue;
    }

    auto details = evaluate(key, user, setting->second, fetchTime);
    const ValueType* valuePtr = get_if<ValueType>(&details.value);
    if (valuePtr)
        return *valuePtr;

    LOG_ERROR << "Evaluating getValue(\"" << key << "\") failed. Returning defaultValue: " << defaultValue << " .";
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
    LOG_DEBUG << "Switched to ONLINE mode.";
}

void ConfigCatClient::setOffline() {
    if (configService) {
        configService->setOffline();
    }
    LOG_DEBUG << "Switched to OFFLINE mode.";
}

bool ConfigCatClient::isOffline() {
    if (configService) {
        return configService->isOffline();
    }
    return true;
}

} // namespace configcat

