#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include "keyvalue.hpp"

namespace configcat {

class ConfigCatUser;

class ConfigCatClient {
public:

    /**
     Gets a value from the configuration identified by the given `key`.

     - Parameter key: the identifier of the configuration value.
     - Parameter defaultValue: in case of any failure, this value will be returned.
     - Parameter user: the user object to identify the caller.
     */
    template<typename ValueType>
    ValueType getValue(const std::string& key, const ValueType& defaultValue, const ConfigCatUser* user = nullptr) const;
    std::string getValue(const std::string& key, const char* defaultValue, const ConfigCatUser* user = nullptr) const;

    // Gets all the setting keys.
    std::string getAllKeys() const;

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
};

} // namespace configcat
