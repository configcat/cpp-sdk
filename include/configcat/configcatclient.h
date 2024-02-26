#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <memory>
#include <mutex>
#include "keyvalue.h"
#include "configcatoptions.h"
#include "refreshresult.h"
#include "settingresult.h"
#include "evaluationdetails.h"


namespace configcat {

class ConfigCatUser;
class ConfigCatLogger;
class ConfigFetcher;
class RolloutEvaluator;
class FlagOverrides;
class ConfigService;


class ConfigCatClient {
public:
    // Creates a new or gets an already existing [ConfigCatClient] for the given [sdkKey].
    static ConfigCatClient* get(const std::string& sdkKey, const ConfigCatOptions* options = nullptr);

    // Closes an individual [ConfigCatClient] instance.
    static void close(ConfigCatClient* client);

    // Closes all [ConfigCatClient] instances.
    static void closeAll();

    // Returns count of [ConfigCatClient] instances.
    static size_t instanceCount();

    /**
     * Gets a value from the configuration identified by the given `key`.
     *
     * Parameter [key]: the identifier of the configuration value.
     * Parameter [defaultValue]: in case of any failure, this value will be returned.
     * Parameter [user]: the user object to identify the caller.
     */
    bool getValue(const std::string& key, bool defaultValue, const ConfigCatUser* user = nullptr) const;
    int32_t getValue(const std::string& key, int32_t defaultValue, const ConfigCatUser* user = nullptr) const;
    double getValue(const std::string& key, double defaultValue, const ConfigCatUser* user = nullptr) const;
    std::string getValue(const std::string& key, const char* defaultValue, const ConfigCatUser* user = nullptr) const;
    std::string getValue(const std::string& key, const std::string& defaultValue, const ConfigCatUser* user = nullptr) const;

    /**
     * Gets the value of a feature flag or setting as std::shared_ptr<Value> identified by the given [key].
     * In case of any failure, std::nullopt will be returned. The [user] param identifies the caller.
     */
    std::optional<Value> getValue(const std::string& key, const ConfigCatUser* user = nullptr) const;

    /**
     * Gets the value and evaluation details of a feature flag or setting identified by the given `key`.
     *
     * Parameter [key]: the identifier of the configuration value.
     * Parameter [defaultValue]: in case of any failure, this value will be returned.
     * Parameter [user]: the user object to identify the caller.
     */
    EvaluationDetails getValueDetails(const std::string& key, bool defaultValue, const ConfigCatUser* user = nullptr) const;
    EvaluationDetails getValueDetails(const std::string& key, int32_t defaultValue, const ConfigCatUser* user = nullptr) const;
    EvaluationDetails getValueDetails(const std::string& key, double defaultValue, const ConfigCatUser* user = nullptr) const;
    EvaluationDetails getValueDetails(const std::string& key, const std::string& defaultValue, const ConfigCatUser* user = nullptr) const;
    EvaluationDetails getValueDetails(const std::string& key, const char* defaultValue, const ConfigCatUser* user = nullptr) const;

    // Gets all the setting keys.
    std::vector<std::string> getAllKeys() const;

    // Gets the key of a setting and it's value identified by the given Variation ID (analytics)
    std::shared_ptr<KeyValue> getKeyAndValue(const std::string& variationId) const;

    // Gets the values of all feature flags or settings.
    std::unordered_map<std::string, Value> getAllValues(const ConfigCatUser* user = nullptr) const;

    // Gets the values along with evaluation details of all feature flags and settings.
    std::vector<EvaluationDetails> getAllValueDetails(const ConfigCatUser* user = nullptr) const;

    // Initiates a force refresh synchronously on the cached configuration.
    void forceRefresh();

    // Sets the default user.
    void setDefaultUser(std::shared_ptr<ConfigCatUser> user) {
        defaultUser = user;
    }

    // Sets the default user to nullptr.
    void clearDefaultUser() {
        defaultUser.reset();
    }

    // Configures the SDK to allow HTTP requests.
    void setOnline();

    // Configures the SDK to not initiate HTTP requests and work only from its cache.
    void setOffline();

    // true when the SDK is configured not to initiate HTTP requests, otherwise false.
    bool isOffline() const;

    // Gets the Hooks object for subscribing events.
    std::shared_ptr<Hooks> getHooks() { return hooks; }

private:
    ConfigCatClient(const std::string& sdkKey, const ConfigCatOptions& options);

    template<typename ValueType>
    ValueType _getValue(const std::string& key, const ValueType& defaultValue, const ConfigCatUser* user = nullptr) const;

    template<typename ValueType>
    EvaluationDetails _getValueDetails(const std::string& key, ValueType defaultValue, const ConfigCatUser* user = nullptr) const;

    SettingResult getSettings() const;

    EvaluationDetails evaluate(const std::string& key,
                               const ConfigCatUser* user,
                               const Setting& setting,
                               double fetchTime) const;

    std::shared_ptr<Hooks> hooks;
    std::shared_ptr<ConfigCatLogger> logger;
    std::shared_ptr<ConfigCatUser> defaultUser;
    std::unique_ptr<RolloutEvaluator> rolloutEvaluator;
    std::shared_ptr<OverrideDataSource> overrideDataSource;
    std::unique_ptr<ConfigService> configService;

    static std::mutex instancesMutex;
    static std::unordered_map<std::string, std::unique_ptr<ConfigCatClient>> instances;
};

} // namespace configcat
