#include <assert.h>

#include "configcat/configcatclient.h"
#include "configcat/configcatuser.h"
#include "configcat/log.h"

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
    return "string";
}


void ConfigCatClient::forceRefresh() {

}

} // namespace configcat

