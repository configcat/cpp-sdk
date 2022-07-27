#include <assert.h>

#include "configcat/configcatclient.h"
#include "configcat/configcatuser.h"
#include "configcat/log.h"
#include "configcat/configfetcher.h"
#include "configcat/rolloutevaluator.h"
#include "configcat/configjsoncache.h"
#include "configcat/refreshpolicy.h"

using namespace std;

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
    refreshPolicy = options.mode->createRefreshPolicy(*configFetcher, *configJsonCache);
}

const std::shared_ptr<std::unordered_map<std::string, Setting>> ConfigCatClient::getSettings() const {
    // TODO: override

    auto config = refreshPolicy->getConfiguration();
    return config->entries;
}

template<>
bool ConfigCatClient::getValue(const std::string& key, const bool& defaultValue, const ConfigCatUser* user) const {
    LOG_DEBUG << "getValue bool";
    return true;
}

template<>
std::string ConfigCatClient::getValue(const std::string& key, const std::string& defaultValue, const ConfigCatUser* user) const {
    return getValue(key, defaultValue.c_str(), user);
}

template<>
int ConfigCatClient::getValue(const std::string& key, const int& defaultValue, const ConfigCatUser* user) const {
    return 42;
}

template<>
double ConfigCatClient::getValue(const std::string& key, const double& defaultValue, const ConfigCatUser* user) const {
    return 42.3;
}

std::string ConfigCatClient::getValue(const std::string& key, char* defaultValue, const ConfigCatUser* user) const {
    return getValue(key, (const char*)defaultValue, user);
}

std::string ConfigCatClient::getValue(const std::string& key, const char* defaultValue, const ConfigCatUser* user) const {
    auto settingsPtr = getSettings();
    if (!settingsPtr || settingsPtr->empty()) {
        LOG_ERROR << "Config JSON is not present. Returning defaultValue: " << defaultValue << ".";
        return defaultValue;
    }
    auto settings = *settingsPtr;

    auto setting = settings.find(key);
    if (setting == settings.end()) {
        vector<string> keys;
        keys.reserve(settings.size());
        for (auto keyValue : settings) {
            keys.emplace_back(keyValue.first);
        }
        LOG_ERROR << "Value not found for key " << key << ". Here are the available keys: " << keys;
        return defaultValue;
    }

    // TODO: evaluation
    const string* valuePtr = get_if<string>(&setting->second.value);
    if (valuePtr)
        return *valuePtr;

    return defaultValue;
}


void ConfigCatClient::forceRefresh() {
    refreshPolicy->refresh();
}

} // namespace configcat

