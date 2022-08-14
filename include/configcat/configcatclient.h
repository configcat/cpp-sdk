#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <memory>
#include "keyvalue.h"
#include "configcatoptions.h"

namespace configcat {

class ConfigJsonCache;
class ConfigCatUser;
class ConfigFetcher;
class RolloutEvaluator;
class RefreshPolicy;
class FlagOverrides;
class ConfigService;

// Version string
extern const char* const version;

class ConfigCatClient {
public:
    // Creates a new or gets an already existing [ConfigCatClient] for the given [sdkKey].
    static ConfigCatClient* get(const std::string& sdkKey, const ConfigCatOptions& options = {});

    // Closes an individual [ConfigCatClient] instance.
    static void close(ConfigCatClient* client);

    // Closes all [ConfigCatClient] instances.
    static void close();

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
    unsigned int getValue(const std::string& key, unsigned int defaultValue, const ConfigCatUser* user = nullptr) const;
    double getValue(const std::string& key, double defaultValue, const ConfigCatUser* user = nullptr) const;
    int getValue(const std::string& key, int defaultValue, const ConfigCatUser* user = nullptr) const;
    std::string getValue(const std::string& key, char* defaultValue, const ConfigCatUser* user = nullptr) const;
    std::string getValue(const std::string& key, const char* defaultValue, const ConfigCatUser* user = nullptr) const;

    /**
     * Gets the value of a feature flag or setting as std::shared_ptr<Value> identified by the given [key].
     * In case of any failure, nullptr will be returned. The [user] param identifies the caller.
     */
    std::shared_ptr<Value> getValue(const std::string& key, const ConfigCatUser* user = nullptr) const;

    // Gets all the setting keys.
    std::vector<std::string> getAllKeys() const;

    // Gets the Variation ID (analytics) of a feature flag or setting based on it's key.
    std::string getVariationId(const std::string& key, const std::string& defaultVariationId, const ConfigCatUser* user = nullptr) const;

    // Gets the Variation IDs (analytics) of all feature flags or settings.
    std::vector<std::string> getAllVariationIds(const ConfigCatUser* user = nullptr) const;

    // Gets the key of a setting and it's value identified by the given Variation ID (analytics)
    KeyValue getKeyAndValue(const std::string& variationId) const;

    // Gets the values of all feature flags or settings.
    std::unordered_map<std::string, Value> getAllValues(const ConfigCatUser* user = nullptr) const;

    // Initiates a force refresh synchronously on the cached configuration.
    void forceRefresh();

private:
    ConfigCatClient(const std::string& sdkKey, const ConfigCatOptions& options);

    template<typename ValueType>
    ValueType _getValue(const std::string& key, const ValueType& defaultValue, const ConfigCatUser* user = nullptr) const;
    const std::shared_ptr<std::unordered_map<std::string, Setting>> getSettings() const;

    std::unique_ptr<RolloutEvaluator> rolloutEvaluator;
    std::shared_ptr<FlagOverrides> override;
    std::unique_ptr<ConfigService> configService;

    static std::unordered_map<std::string, std::unique_ptr<ConfigCatClient>> instanceRepository;
};

} // namespace configcat
