#include <assert.h>

#include "configcat/configcatclient.h"
#include "configcat/configcatuser.h"
#include "configcat/log.h"
#include "rolloutevaluator.h"
#include "configservice.h"
#include "configcat/flagoverrides.h"
#include "configcat/overridedatasource.h"
#include "configcat/configcatlogger.h"
#include "configcat/consolelogger.h"

using namespace std;

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
        configService = make_unique<ConfigService>(sdkKey, logger, hooks, options);
    }
}

const std::shared_ptr<std::unordered_map<std::string, Setting>> ConfigCatClient::getSettings() const {
    if (overrideDataSource) {
        switch (overrideDataSource->getBehaviour()) {
            case LocalOnly:
                return overrideDataSource->getOverrides();
            case LocalOverRemote: {
                auto remote = configService ? configService->getSettings() : nullptr;
                auto local = overrideDataSource->getOverrides();
                auto result = make_shared<std::unordered_map<std::string, Setting>>();
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

                return result;
            }
            case RemoteOverLocal:
                auto remote = configService ? configService->getSettings() : nullptr;
                auto local = overrideDataSource->getOverrides();
                auto result = make_shared<std::unordered_map<std::string, Setting>>();
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

                return result;
        }
    }

    return configService ? configService->getSettings() : nullptr;
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
    auto settings = getSettings();
    if (!settings || settings->empty()) {
        LOG_ERROR << "Config JSON is not present.";
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

    auto [value, variationId] = rolloutEvaluator->evaluate(setting->second, key, user);
    return make_shared<Value>(value);
}

std::vector<std::string> ConfigCatClient::getAllKeys() const {
    auto settings = getSettings();
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

string ConfigCatClient::getVariationId(const string& key, const string& defaultVariationId, const ConfigCatUser* user) const {
    auto settings = getSettings();
    if (!settings || settings->empty()) {
        LOG_ERROR << "Config JSON is not present. Returning defaultVariationId: " << defaultVariationId << ".";
        return defaultVariationId;
    }

    auto setting = settings->find(key);
    if (setting == settings->end()) {
        vector<string> keys;
        keys.reserve(settings->size());
        for (auto keyValue : *settings) {
            keys.emplace_back(keyValue.first);
        }
        LOG_ERROR << "Variation ID not found for key " << key << ". Here are the available keys: " << keys
                  << " Returning defaultVariationId: " << defaultVariationId << ".";
        return defaultVariationId;
    }

    auto [value, variationId] = rolloutEvaluator->evaluate(setting->second, key, user);
    return variationId;
}

std::vector<std::string> ConfigCatClient::getAllVariationIds(const ConfigCatUser* user) const {
    auto settings = getSettings();
    if (!settings || settings->empty()) {
        return {};
    }

    vector<string> variationIds;
    variationIds.reserve(settings->size());
    for (auto keyValue : *settings) {
        auto [value, variationId] = rolloutEvaluator->evaluate(keyValue.second, keyValue.first, user);
        variationIds.emplace_back(variationId);
    }

    return variationIds;
}

std::shared_ptr<KeyValue> ConfigCatClient::getKeyAndValue(const std::string& variationId) const {
    auto settings = getSettings();
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
    auto settings = getSettings();
    if (!settings || settings->empty()) {
        return {};
    }

    std::unordered_map<std::string, Value> result;
    for (auto keyValue : *settings) {
        auto& key = keyValue.first;
        auto [value, variationId] = rolloutEvaluator->evaluate(keyValue.second, key, user);
        result.insert({key, value});
    }

    return result;
}

template<typename ValueType>
ValueType ConfigCatClient::_getValue(const std::string& key, const ValueType& defaultValue, const ConfigCatUser* user) const {
    auto settings = getSettings();
    if (!settings || settings->empty()) {
        LOG_ERROR << "Config JSON is not present. Returning defaultValue: " << defaultValue << " .";
        return defaultValue;
    }

    auto setting = settings->find(key);
    if (setting == settings->end()) {
        vector<string> keys;
        keys.reserve(settings->size());
        for (auto keyValue : *settings) {
            keys.emplace_back(keyValue.first);
        }
        LOG_ERROR << "Value not found for key " << key << ". Here are the available keys: " << keys
                  << " Returning defaultValue: " << defaultValue << " .";
        return defaultValue;
    }

    auto [value, variationId] = rolloutEvaluator->evaluate(setting->second, key, user);
    const ValueType* valuePtr = get_if<ValueType>(&value);
    if (valuePtr)
        return *valuePtr;

    LOG_ERROR << "Evaluating getValue(\"" << key << "\") failed. Returning defaultValue: " << defaultValue << " .";
    return defaultValue;
}

void ConfigCatClient::forceRefresh() {
    if (configService) {
        configService->refresh();
    }
}

} // namespace configcat

