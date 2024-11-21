#include <assert.h>
#include <chrono>
#include <regex>

#include "configcat/configcatclient.h"
#include "configcat/configcatuser.h"
#include "configcat/log.h"
#include "rolloutevaluator.h"
#include "configservice.h"
#include "configcat/flagoverrides.h"
#include "configcat/overridedatasource.h"
#include "configcatlogger.h"
#include "configcat/consolelogger.h"

using namespace std;
using namespace std::chrono;

namespace configcat {

bool isValidSdkKey(const string& sdkKey, bool customBaseUrl) {
    static constexpr char proxyPrefix[] = "configcat-proxy/";

    if (customBaseUrl && sdkKey.size() >= sizeof(proxyPrefix) && starts_with(sdkKey, proxyPrefix)) {
        return true;
    }

    static const regex re("^(?:configcat-sdk-1/)?[^/]{22}/[^/]{22}$", regex_constants::ECMAScript);
    return regex_match(sdkKey, re);
}

// NOTE: make_shared doesn't work with private ctors but we can use this workaround to avoid double allocation
// (see also https://stackoverflow.com/a/8147213/8656352)
struct ConfigCatClient::MakeSharedEnabler : ConfigCatClient {
    MakeSharedEnabler(const std::string& sdkKey, const ConfigCatOptions& options) : ConfigCatClient(sdkKey, options) {}
};

std::shared_ptr<ConfigCatClient> ConfigCatClient::get(const std::string& sdkKey, const ConfigCatOptions* options) {
    if (sdkKey.empty()) {
        throw invalid_argument("SDK Key cannot be empty.");
    }

    ConfigCatOptions defaultOptions;
    const auto& actualOptions = options ? *options : defaultOptions;

    const auto& flagOverrides = actualOptions.flagOverrides;
    if (!flagOverrides || flagOverrides->getBehavior() != OverrideBehaviour::LocalOnly) {
        const auto customBaseUrl = !actualOptions.baseUrl.empty();
        if (!isValidSdkKey(sdkKey, customBaseUrl)) {
            throw invalid_argument(string_format("SDK Key '%s' is invalid.", sdkKey.c_str()));
        }
    }

    lock_guard<mutex> lock(getInstancesMutex());
    auto& instances = getInstances();
    auto client = instances.find(sdkKey);
    if (client == instances.end()) {
        client = instances.insert({sdkKey, make_shared<ConfigCatClient::MakeSharedEnabler>(sdkKey, actualOptions)}).first;
    } else if (options) {
        LOG_WARN_OBJECT(client->second->logger, 3000) <<
            "There is an existing client instance for the specified SDK Key. "
            "No new client instance will be created and the specified options are ignored. "
            "Returning the existing client instance. SDK Key: '" << sdkKey << "'.";
    }
    return client->second;
}

void ConfigCatClient::close(const std::shared_ptr<ConfigCatClient>& client) {
    if (!client) {
        return;
    }

    {
        lock_guard<mutex> lock(getInstancesMutex());
        auto& instances = getInstances();

        client->closeResources();

        for (auto it = instances.begin(); it != instances.end(); ++it) {
            if (it->second == client) {
                instances.erase(it);
                return;
            }
        }
    }

    LOG_ERROR_OBJECT(client->logger, 0) << "Client does not exist.";
    assert(false);
}

void ConfigCatClient::closeAll() {
    lock_guard<mutex> lock(getInstancesMutex());
    auto& instances = getInstances();

    for (const auto& [_, instance] : instances) {
        instance->closeResources();
    }

    instances.clear();
}

size_t ConfigCatClient::instanceCount() {
    lock_guard<mutex> lock(getInstancesMutex());
    auto& instances = getInstances();

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

void ConfigCatClient::closeResources() {
    configService.reset(); // stop polling by destroying configService
}

SettingResult ConfigCatClient::getSettings() const {
    if (overrideDataSource) {
        switch (overrideDataSource->getBehaviour()) {
            case LocalOnly:
                return { overrideDataSource->getOverrides(), kDistantPast };
            case LocalOverRemote: {
                auto settingResult = configService ? configService->getSettings() : SettingResult{nullptr, kDistantPast};
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
                auto settingResult = configService ? configService->getSettings() : SettingResult{nullptr, kDistantPast};
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

    return configService ? configService->getSettings() : SettingResult{nullptr, kDistantPast};
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
    return _getValue<string>(key, defaultValue, user);
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
EvaluationDetails<ValueType> ConfigCatClient::_getValueDetails(const std::string& key, const ValueType& defaultValue, const std::shared_ptr<ConfigCatUser>& user) const {
    try {
        auto settingResult = getSettings();
        auto& settings = settingResult.settings;
        auto& fetchTime = settingResult.fetchTime;
        if (!settings) {
            LogEntry logEntry(logger, LOG_LEVEL_ERROR, 1000);
            if constexpr (is_same_v<ValueType, optional<Value>>) {
                logEntry << "Config JSON is not present when evaluating setting '" << key << "'. Returning std::nullopt.";
            } else {
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
            for (const auto& [key, _] : *settings) {
                keys.emplace_back(key);
            }
            LogEntry logEntry(logger, LOG_LEVEL_ERROR, 1001);
            if constexpr (is_same_v<ValueType, optional<Value>>) {
                logEntry <<
                    "Failed to evaluate setting '" << key << "' (the key was not found in config JSON). "
                    "Returning the `defaultValue` parameter that you specified in your application: '" << defaultValue << "'. "
                    "Available keys: " << keys << ".";
            } else {
                logEntry <<
                    "Failed to evaluate setting '" << key << "' (the key was not found in config JSON). "
                    "Returning std::nullopt. Available keys: " << keys << ".";
            }
            auto details = EvaluationDetails<ValueType>::fromError(key, defaultValue, logEntry.getMessage());
            hooks->invokeOnFlagEvaluated(details);
            return details;
        }

        const auto& effectiveUser = user ? user : defaultUser;
        return evaluate<ValueType>(key, defaultValue, effectiveUser, setting->second, settings, fetchTime);
    }
    catch (...) {
        auto ex = std::current_exception();
        LogEntry logEntry(logger, LOG_LEVEL_ERROR, 1002, ex);
        logEntry << "Error occurred in the `getValueDetails` method while evaluating setting '" << key << "'. ";
        if constexpr (is_same_v<ValueType, optional<Value>>) {
            logEntry << "Returning std::nullopt.";
        } else {
            logEntry << "Returning the `defaultValue` parameter that you specified in your application: '" << defaultValue << "'.";
        }
        auto details = EvaluationDetails<ValueType>::fromError(key, defaultValue, logEntry.getMessage(), ex);
        hooks->invokeOnFlagEvaluated(details);
        return details;
    }
}

std::vector<std::string> ConfigCatClient::getAllKeys() const {
    try {
        auto settingResult = getSettings();
        auto& settings = settingResult.settings;
        auto& fetchTime = settingResult.fetchTime;
        if (!settings) {
            LOG_ERROR(1000) << "Config JSON is not present. Returning empty list.";
            return {};
        }

        vector<string> keys;
        keys.reserve(settings->size());
        for (const auto& [key, _] : *settings) {
            keys.emplace_back(key);
        }
        return keys;
    }
    catch (...) {
        LogEntry logEntry(logger, LOG_LEVEL_ERROR, 1002, std::current_exception());
        logEntry << "Error occurred in the `getAllKeys` method. Returning empty list.";
        return {};
    }
}

std::optional<KeyValue> ConfigCatClient::getKeyAndValue(const std::string& variationId) const {
    try {
        auto settingResult = getSettings();
        auto& settings = settingResult.settings;
        if (!settings) {
            LOG_ERROR(1000) << "Config JSON is not present. Returning std::nullopt.";
            return nullopt;
        }

        for (const auto& [key, setting] : *settings) {
            const auto settingType = setting.getTypeChecked();

            if (setting.variationId == variationId) {
                return KeyValue(key, *setting.value.toValueChecked(settingType));
            }

            for (const auto& targetingRule : setting.targetingRules) {
                if (const auto simpleValuePtr = get_if<SettingValueContainer>(&targetingRule.then); simpleValuePtr) {
                    if (simpleValuePtr->variationId == variationId) {
                        return KeyValue(key, *simpleValuePtr->value.toValueChecked(settingType));
                    }
                } else if (const auto percentageOptionsPtr = get_if<PercentageOptions>(&targetingRule.then);
                    percentageOptionsPtr && !percentageOptionsPtr->empty()) {

                    for (const auto& percentageOption : *percentageOptionsPtr) {
                        if (percentageOption.variationId == variationId) {
                            return KeyValue(key, *percentageOption.value.toValueChecked(settingType));
                        }
                    }
                } else {
                    throw runtime_error("Targeting rule THEN part is missing or invalid.");
                }
            }

            for (const auto& percentageOption : setting.percentageOptions) {
                if (percentageOption.variationId == variationId) {
                    return KeyValue(key, *percentageOption.value.toValueChecked(settingType));
                }
            }
        }

        LOG_ERROR(2011) << "Could not find the setting for the specified variation ID: '" << variationId << "'.";
        return std::nullopt;
    }
    catch (...) {
        LogEntry logEntry(logger, LOG_LEVEL_ERROR, 1002, std::current_exception());
        logEntry << "Error occurred in the `getKeyAndValue` method. Returning std::nullopt.";
        return std::nullopt;
    }
}

std::unordered_map<std::string, Value> ConfigCatClient::getAllValues(const std::shared_ptr<ConfigCatUser>& user) const {
    try {
        auto settingResult = getSettings();
        auto& settings = settingResult.settings;
        auto& fetchTime = settingResult.fetchTime;
        if (!settings) {
            LOG_ERROR(1000) << "Config JSON is not present. Returning empty map.";
            return {};
        }

        std::unordered_map<std::string, Value> result;
        const auto& effectiveUser = user ? user : defaultUser;
        for (const auto& [key, setting] : *settings) {
            auto details = evaluate<Value>(key, nullopt, effectiveUser, setting, settings, fetchTime);
            result.insert({ key, std::move(details.value) });
        }

        return result;
    }
    catch (...) {
        LogEntry logEntry(logger, LOG_LEVEL_ERROR, 1002, std::current_exception());
        logEntry << "Error occurred in the `getAllValues` method. Returning empty map.";
        return {};
    }
}

std::vector<EvaluationDetails<Value>> ConfigCatClient::getAllValueDetails(const std::shared_ptr<ConfigCatUser>& user) const {
    try {
        auto settingResult = getSettings();
        auto& settings = settingResult.settings;
        auto& fetchTime = settingResult.fetchTime;
        if (!settings) {
            LOG_ERROR(1000) << "Config JSON is not present. Returning empty list.";
            return {};
        }

        std::vector<EvaluationDetails<Value>> result;
        const auto& effectiveUser = user ? user : defaultUser;
        for (const auto& [key, setting] : *settings) {
            result.push_back(evaluate<Value>(key, nullopt, effectiveUser, setting, settings, fetchTime));
        }

        return result;
    }
    catch (...) {
        LogEntry logEntry(logger, LOG_LEVEL_ERROR, 1002, std::current_exception());
        logEntry << "Error occurred in the `getAllValueDetails` method. Returning empty list.";
        return {};
    }
}

template<typename ValueType>
ValueType ConfigCatClient::_getValue(const std::string& key, const ValueType& defaultValue, const std::shared_ptr<ConfigCatUser>& user) const {
    try {
        auto settingResult = getSettings();
        auto& settings = settingResult.settings;
        auto& fetchTime = settingResult.fetchTime;
        if (!settings) {
            LogEntry logEntry(logger, LOG_LEVEL_ERROR, 1000);
            if constexpr (is_same_v<ValueType, optional<Value>>) {
                logEntry << "Config JSON is not present when evaluating setting '" << key << "'. Returning std::nullopt.";
            } else {
                logEntry << "Config JSON is not present when evaluating setting '" << key << "'. Returning the `defaultValue` parameter that you specified in your application: '" << defaultValue << "'.";
            }
            hooks->invokeOnFlagEvaluated(EvaluationDetails<ValueType>::fromError(key, defaultValue, logEntry.getMessage()));
            return defaultValue;
        }

        auto setting = settings->find(key);
        if (setting == settings->end()) {
            vector<string> keys;
            keys.reserve(settings->size());
            for (const auto& [key, _] : *settings) {
                keys.emplace_back(key);
            }
            LogEntry logEntry(logger, LOG_LEVEL_ERROR, 1001);
            if constexpr (is_same_v<ValueType, optional<Value>>) {
                logEntry <<
                    "Failed to evaluate setting '" << key << "' (the key was not found in config JSON). "
                    "Returning the `defaultValue` parameter that you specified in your application: '" << defaultValue << "'. "
                    "Available keys: " << keys << ".";
            } else {
                logEntry <<
                    "Failed to evaluate setting '" << key << "' (the key was not found in config JSON). "
                    "Returning std::nullopt. Available keys: " << keys << ".";
            }
            hooks->invokeOnFlagEvaluated(EvaluationDetails<ValueType>::fromError(key, defaultValue, logEntry.getMessage()));
            return defaultValue;
        }

        const auto& effectiveUser = user ? user : defaultUser;
        auto details = evaluate<ValueType>(key, defaultValue, effectiveUser, setting->second, settings, fetchTime);

        return std::move(details.value);
    }
    catch (...) {
        auto ex = std::current_exception();
        LogEntry logEntry(logger, LOG_LEVEL_ERROR, 1002, ex);
        logEntry << "Error occurred in the `getValue` method while evaluating setting '" << key << "'. ";
        if constexpr (is_same_v<ValueType, optional<Value>>) {
            logEntry << "Returning std::nullopt.";
        } else {
            logEntry << "Returning the `defaultValue` parameter that you specified in your application: '" << defaultValue << "'.";
        }
        auto details = EvaluationDetails<ValueType>::fromError(key, defaultValue, logEntry.getMessage(), ex);
        hooks->invokeOnFlagEvaluated(details);
        return defaultValue;
    }
}

template<typename ValueType>
EvaluationDetails<ValueType> ConfigCatClient::evaluate(const std::string& key,
                                                       const std::optional<Value>& defaultValue,
                                                       const std::shared_ptr<ConfigCatUser>& effectiveUser,
                                                       const Setting& setting,
                                                       const std::shared_ptr<Settings>& settings,
                                                       double fetchTime) const {
    EvaluateContext evaluateContext(key, setting, effectiveUser, settings);
    std::optional<Value> returnValue;
    auto evaluateResult = rolloutEvaluator->evaluate(defaultValue, evaluateContext, returnValue);

    ValueType value;
    if constexpr (is_same_v<ValueType, bool> || is_same_v<ValueType, string> || is_same_v<ValueType, int32_t> || is_same_v<ValueType, double>) {
        // RolloutEvaluator::evaluate makes sure that this variant access is always valid.
        value = std::get<ValueType>(*returnValue);
    } else if constexpr (is_same_v<ValueType, Value>) {
        value = *returnValue;
    } else if constexpr (is_same_v<ValueType, optional<Value>>) {
        value = returnValue;
    } else {
        static_assert(always_false_v<ValueType>, "Unsupported value type.");
    }

    EvaluationDetails<ValueType> details(key,
                                 value,
                                 evaluateResult.selectedValue.variationId,
                                 time_point<system_clock, duration<double>>(duration<double>(fetchTime)),
                                 effectiveUser,
                                 false,
                                 nullopt,
                                 nullptr,
                                 evaluateResult.targetingRule,
                                 evaluateResult.percentageOption);
    hooks->invokeOnFlagEvaluated(details);
    return details;
}

RefreshResult ConfigCatClient::forceRefresh() {
    try {
        return configService
            ? configService->refresh()
            : RefreshResult{"Client is configured to use the LocalOnly override behavior or has been closed, which prevents making HTTP requests."};
    } catch (...) {
        auto ex = std::current_exception();
        LogEntry logEntry(logger, LOG_LEVEL_ERROR, 1003, ex);
        logEntry << "Error occurred in the `forceRefresh` method.";
        return RefreshResult{logEntry.getMessage(), ex};
    }
}

void ConfigCatClient::setOnline() {
    if (configService) {
        configService->setOnline();
    } else {
        LOG_WARN(3202) << "Client is configured to use the `LocalOnly` override behavior or has been closed, thus `setOnline()` has no effect.";
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

