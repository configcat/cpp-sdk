#pragma once

#include <string>
#include <map>
#include <functional>
#include <vector>
#include "datagovernance.h"
#include "pollingmode.h"
#include "configcache.h"
#include "httpsessionadapter.h"
#include "flagoverrides.h"
#include "log.h"

namespace configcat {

struct ProxyAuthentication {
    std::string user;
    std::string password;
};

// Hooks for events sent by `ConfigCatClient`.
class Hooks {
public:
    explicit Hooks(const std::function<void()>& onClientReady = nullptr,
          const std::function<void(std::string)>& onConfigChanged = nullptr,
          const std::function<void(std::string)>& onFlagEvaluated = nullptr,
          const std::function<void(std::string)>& onError = nullptr) {
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
        onClientReadyCallbacks.push_back(callback);
    }

    void addOnConfigChanged(const std::function<void(std::string)>& callback) {
        onConfigChangedCallbacks.push_back(callback);
    }

    void addOnFlagEvaluated(const std::function<void(std::string)>& callback) {
        onFlagEvaluatedCallbacks.push_back(callback);
    }

    void addOnError(const std::function<void(std::string)>& callback) {
        onErrorCallbacks.push_back(callback);
    }

    void invokeOnClientReady() {
        for (auto& callback : onClientReadyCallbacks) {
            try {
                callback();
            } catch (const std::exception& e) {
                std::string error = "Exception occurred during invokeOnClientReady callback: " + std::string(e.what());
                invokeOnError(error);
                LOG_ERROR << error;
            }
        }
    }

    void invokeOnConfigChanged(std::string config) {
        for (auto& callback : onConfigChangedCallbacks) {
            try {
                callback(config);
            } catch (const std::exception& e) {
                std::string error = "Exception occurred during invokeOnConfigChanged callback: " + std::string(e.what());
                invokeOnError(error);
                LOG_ERROR << error;
            }
        }
    }

    void invokeOnFlagEvaluated(std::string evaluation_details) {
        for (auto& callback : onFlagEvaluatedCallbacks) {
            try {
                callback(evaluation_details);
            } catch (const std::exception& e) {
                std::string error = "Exception occurred during invokeOnFlagEvaluated callback: " + std::string(e.what());
                invokeOnError(error);
                LOG_ERROR << error;
            }
        }
    }

    void invokeOnError(std::string error) {
        for (auto& callback : onErrorCallbacks) {
            try {
                callback(error);
            } catch (const std::exception& e) {
                LOG_ERROR << "Exception occurred during invokeOnError callback: " << e.what();
            }
        }
    }

    void clear() {
        onClientReadyCallbacks.clear();
        onConfigChangedCallbacks.clear();
        onFlagEvaluatedCallbacks.clear();
        onErrorCallbacks.clear();
    }

private:
    std::vector<std::function<void()>> onClientReadyCallbacks;
    std::vector<std::function<void(std::string)>> onConfigChangedCallbacks;
    std::vector<std::function<void(std::string)>> onFlagEvaluatedCallbacks;
    std::vector<std::function<void(std::string)>> onErrorCallbacks;
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
    std::shared_ptr<FlagOverrides> override;

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

    /// Indicates whether the SDK should be initialized in offline mode or not.
    bool offline = false;
};

} // namespace configcat
