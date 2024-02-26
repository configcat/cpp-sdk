#pragma once

#include <string>
#include <map>
#include <functional>
#include <vector>
#include <mutex>
#include "datagovernance.h"
#include "pollingmode.h"
#include "configcache.h"
#include "config.h"
#include "httpsessionadapter.h"
#include "flagoverrides.h"
#include "log.h"
#include "evaluationdetails.h"
#include "proxyauthentication.h"

namespace configcat {

// Hooks for events sent by `ConfigCatClient`.
class Hooks {
public:
    explicit Hooks(const std::function<void()>& onClientReady = nullptr,
          const std::function<void(std::shared_ptr<Settings>)>& onConfigChanged = nullptr,
          const std::function<void(const EvaluationDetailsBase&)>& onFlagEvaluated = nullptr,
          const std::function<void(const std::string&)>& onError = nullptr) {
        if (onClientReady) {
            onClientReadyCallbacks.push_back(onClientReady);
        }
        if (onConfigChanged) {
            onConfigChangedCallbacks.push_back(onConfigChanged);
        }
        if (onFlagEvaluated) {
            onFlagEvaluatedCallbacks.push_back(onFlagEvaluated);
        }
        if (onError) {
            onErrorCallbacks.push_back(onError);
        }
    }

    void addOnClientReady(const std::function<void()>& callback) {
        std::lock_guard<std::mutex> lock(mutex);
        onClientReadyCallbacks.push_back(callback);
    }

    void addOnConfigChanged(const std::function<void(std::shared_ptr<Settings>)>& callback) {
        std::lock_guard<std::mutex> lock(mutex);
        onConfigChangedCallbacks.push_back(callback);
    }

    void addOnFlagEvaluated(const std::function<void(const EvaluationDetailsBase&)>& callback) {
        std::lock_guard<std::mutex> lock(mutex);
        onFlagEvaluatedCallbacks.push_back(callback);
    }

    void addOnError(const std::function<void(const std::string&)>& callback) {
        std::lock_guard<std::mutex> lock(mutex);
        onErrorCallbacks.push_back(callback);
    }

    void invokeOnClientReady() {
        std::lock_guard<std::mutex> lock(mutex);
        for (auto& callback : onClientReadyCallbacks) {
            callback();
        }
    }

    void invokeOnConfigChanged(std::shared_ptr<Settings> config) {
        std::lock_guard<std::mutex> lock(mutex);
        for (auto& callback : onConfigChangedCallbacks) {
            callback(config);
        }
    }

    void invokeOnFlagEvaluated(const EvaluationDetailsBase& details) {
        std::lock_guard<std::mutex> lock(mutex);
        for (auto& callback : onFlagEvaluatedCallbacks) {
            callback(details);
        }
    }

    void invokeOnError(const std::string& error) {
        std::lock_guard<std::mutex> lock(mutex);
        for (auto& callback : onErrorCallbacks) {
            callback(error);
        }
    }

    void clear() {
        std::lock_guard<std::mutex> lock(mutex);
        onClientReadyCallbacks.clear();
        onConfigChangedCallbacks.clear();
        onFlagEvaluatedCallbacks.clear();
        onErrorCallbacks.clear();
    }

private:
    std::mutex mutex;
    std::vector<std::function<void()>> onClientReadyCallbacks;
    std::vector<std::function<void(std::shared_ptr<Settings>)>> onConfigChangedCallbacks;
    std::vector<std::function<void(const EvaluationDetailsBase&)>> onFlagEvaluatedCallbacks;
    std::vector<std::function<void(const std::string&)>> onErrorCallbacks;
};

// Configuration options for ConfigCatClient.
struct ConfigCatOptions {
    // The base ConfigCat CDN url.
    std::string baseUrl = "";

    // Default: `DataGovernance::Global`. Set this parameter to be in sync with the
    // Data Governance preference on the [Dashboard](https://app.configcat.com/organization/data-governance).
    // (Only Organization Admins have access)
    DataGovernance dataGovernance = Global;

    // The number of milliseconds to wait for the server to make the initial connection
    uint32_t connectTimeoutMs = 8000; // milliseconds (0 means it never times out during transfer)

    // The number of milliseconds to wait for the server to respond before giving up.
    uint32_t readTimeoutMs = 5000; // milliseconds (0 means it never times out during transfer)

    // The polling mode.
    std::shared_ptr<PollingMode> pollingMode = PollingMode::autoPoll();

    // The cache implementation used to cache the downloaded config.json.
    std::shared_ptr<ConfigCache> configCache;

    // Feature flag and setting overrides.
    std::shared_ptr<FlagOverrides> flagOverrides;

    // Proxy addresses. e.g. { "https": "your_proxy_ip:your_proxy_port" }
    std::map<std::string, std::string> proxies; // Protocol, Proxy url

    // Proxy authentication.
    std::map<std::string, ProxyAuthentication> proxyAuthentications; // Protocol, ProxyAuthentication

    // Custom `HttpSessionAdapter` used by the HTTP calls.
    std::shared_ptr<HttpSessionAdapter> httpSessionAdapter;

    /// The default user, used as fallback when there's no user parameter is passed to the getValue() method.
    std::shared_ptr<ConfigCatUser> defaultUser;

    /// Hooks for events sent by ConfigCatClient.
    std::shared_ptr<Hooks> hooks;

    /// Custom logger.
    std::shared_ptr<ILogger> logger;

    /// Indicates whether the SDK should be initialized in offline mode or not.
    bool offline = false;
};

} // namespace configcat
