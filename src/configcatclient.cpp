#include <assert.h>

#include "configcat/configcatclient.h"
#include "configcat/configcatuser.h"
#include "configcat/log.h"
#include "configcat/configfetcher.h"
#include "rolloutevaluator.h"
#include "configcat/configjsoncache.h"
#include "configcat/refreshpolicy.h"
#include "configcat/flagoverrides.h"
#include "configcat/overridedatasource.h"
#include "version.h"

using namespace std;

const char* const configcat::version = CONFIGCAT_VERSION;

namespace configcat {

std::unordered_map<std::string, std::unique_ptr<ConfigCatClient>> ConfigCatClient::instanceRepository;

ConfigCatClient* ConfigCatClient::get(const std::string& sdkKey, const ConfigCatOptions& options) {
    if (sdkKey.empty()) {
        LOG_ERROR << "The SDK key cannot be empty.";
        assert(false);
        return nullptr;
    }

    auto client = instanceRepository.find(sdkKey);
    if (client == instanceRepository.end()) {
        client = instanceRepository.insert({
            sdkKey,
            move(std::unique_ptr<ConfigCatClient>(new ConfigCatClient(sdkKey, options)))
       }).first;
    }

    return client->second.get();
}

void ConfigCatClient::close(ConfigCatClient* client) {
    for (auto it = instanceRepository.begin(); it != instanceRepository.end(); ++it) {
        if (it->second.get() == client) {
            instanceRepository.erase(it);
            return;
        }
    }

    LOG_ERROR << "Client does not exist.";
    assert(false);
}

void ConfigCatClient::close() {
    instanceRepository.clear();
}

size_t ConfigCatClient::instanceCount() {
    return instanceRepository.size();
}

ConfigCatClient::ConfigCatClient(const std::string& sdkKey, const ConfigCatOptions& options) {
    auto mode = options.mode ? options.mode : PollingMode::autoPoll();
    configJsonCache = make_unique<ConfigJsonCache>(sdkKey, options.cache);
    rolloutEvaluator = make_unique<RolloutEvaluator>();
    configFetcher = make_unique<ConfigFetcher>(sdkKey, mode->getPollingIdentifier(), *configJsonCache, options);
    refreshPolicy = mode->createRefreshPolicy(*configFetcher, *configJsonCache);
    override = options.override;
}

const std::shared_ptr<std::unordered_map<std::string, Setting>> ConfigCatClient::getSettings() const {
    if (override && override->dataSource) {
        switch (override->behaviour) {
            case LocalOnly:
                return override->dataSource->getOverrides();
            case LocalOverRemote: {
                auto remote = refreshPolicy->getConfiguration();
                auto local = override->dataSource->getOverrides();
                auto result = make_shared<std::unordered_map<std::string, Setting>>();
                if (remote && remote->entries) {
                    for (auto& it : *remote->entries) {
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
                auto remote = refreshPolicy->getConfiguration();
                auto local = override->dataSource->getOverrides();
                auto result = make_shared<std::unordered_map<std::string, Setting>>();
                if (local) {
                    for (auto& it : *local) {
                        result->insert_or_assign(it.first, it.second);
                    }
                }
                if (remote && remote->entries) {
                    for (auto& it : *remote->entries) {
                        result->insert_or_assign(it.first, it.second);
                    }
                }

                return result;
        }
    }

    auto config = refreshPolicy->getConfiguration();
    return config->entries;
}

bool ConfigCatClient::getValue(const std::string& key, bool defaultValue, const ConfigCatUser* user) const {
    return _getValue(key, defaultValue, user);
}

unsigned int ConfigCatClient::getValue(const std::string& key, unsigned int defaultValue, const ConfigCatUser* user) const {
    return _getValue(key, defaultValue, user);
}

int ConfigCatClient::getValue(const std::string& key, int defaultValue, const ConfigCatUser* user) const {
    return _getValue(key, defaultValue, user);
}

double ConfigCatClient::getValue(const std::string& key, double defaultValue, const ConfigCatUser* user) const {
    return _getValue(key, defaultValue, user);
}

std::string ConfigCatClient::getValue(const std::string& key, char* defaultValue, const ConfigCatUser* user) const {
    return _getValue(key, string(defaultValue), user);
}

std::string ConfigCatClient::getValue(const std::string& key, const char* defaultValue, const ConfigCatUser* user) const {
    return _getValue(key, string(defaultValue), user);
}

std::shared_ptr<Value> ConfigCatClient::getValue(const std::string& key, const ConfigCatUser* user) const {
    // TODO: Do not duplicate code. Move this into _getValue().
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
        LOG_ERROR << "Value not found for key " << key << ". Here are the available keys: " << keys;
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
        LOG_ERROR << "Variation ID not found for key " << key << ". Here are the available keys: " << keys;
        return defaultVariationId;
    }

    auto [value, variationId] = rolloutEvaluator->evaluate(setting->second, key, user);
    // TODO: on error returning defaultVariationId
    return variationId;
}

template<typename ValueType>
ValueType ConfigCatClient::_getValue(const std::string& key, const ValueType& defaultValue, const ConfigCatUser* user) const {
    auto settings = getSettings();
    if (!settings || settings->empty()) {
        LOG_ERROR << "Config JSON is not present. Returning defaultValue: " << defaultValue << ".";
        return defaultValue;
    }

    auto setting = settings->find(key);
    if (setting == settings->end()) {
        vector<string> keys;
        keys.reserve(settings->size());
        for (auto keyValue : *settings) {
            keys.emplace_back(keyValue.first);
        }
        LOG_ERROR << "Value not found for key " << key << ". Here are the available keys: " << keys;
        return defaultValue;
    }

    auto [value, variationId] = rolloutEvaluator->evaluate(setting->second, key, user);
    const ValueType* valuePtr = get_if<ValueType>(&value);
    if (valuePtr)
        return *valuePtr;

    return defaultValue;
}

void ConfigCatClient::forceRefresh() {
    refreshPolicy->refresh();
}

} // namespace configcat

