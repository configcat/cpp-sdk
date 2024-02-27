#include <assert.h>

#include "configfetcher.h"
#include "configcat/log.h"
#include "configcat/configcatoptions.h"
#include "configcat/configcatlogger.h"
#include "curlnetworkadapter.h"
#include "version.h"
#include "platform.h"
#include "utils.h"

using namespace std;

namespace configcat {

ConfigFetcher::ConfigFetcher(const string& sdkKey, const shared_ptr<ConfigCatLogger>& logger, const string& mode, const ConfigCatOptions& options):
    sdkKey(sdkKey),
    logger(logger),
    mode(mode),
    connectTimeoutMs(options.connectTimeoutMs),
    readTimeoutMs(options.readTimeoutMs),
    proxies(options.proxies),
    proxyAuthentications(options.proxyAuthentications),
    httpSessionAdapter(options.httpSessionAdapter) {
    urlIsCustom = !options.baseUrl.empty();
    url = urlIsCustom
        ? options.baseUrl
        : options.dataGovernance == DataGovernance::Global
            ? kGlobalBaseUrl
            : kEuOnlyBaseUrl;
    userAgent = string("ConfigCat-Cpp/") + mode + "-" + CONFIGCAT_VERSION;

#ifndef CONFIGCAT_EXTERNAL_NETWORK_ADAPTER_ENABLED
    if (!httpSessionAdapter) {
        httpSessionAdapter = make_shared<CurlNetworkAdapter>();
    }
#endif

    if (!httpSessionAdapter || !httpSessionAdapter->init(connectTimeoutMs, readTimeoutMs)) {
        LOG_ERROR(0) << "Cannot initialize httpSessionAdapter.";
        assert(false);
    }
}

ConfigFetcher::~ConfigFetcher() {
}

void ConfigFetcher::close() {
    if (httpSessionAdapter) {
        httpSessionAdapter->close();
    }
}

FetchResponse ConfigFetcher::fetchConfiguration(const std::string& eTag) {
    return executeFetch(eTag, 2);
}

FetchResponse ConfigFetcher::executeFetch(const std::string& eTag, int executeCount) {
    auto response = fetch(eTag);
    auto& preferences = response.entry && response.entry->config ? response.entry->config->preferences : nullopt;

    // If there wasn't a config change or there were no preferences in the config, we return the response
    if (!response.isFetched() || !preferences) {
        return response;
    }

    const auto& baseUrl = preferences->baseUrl.value_or("");
    // If the preferences url is the same as the last called one, just return the response.
    if (!baseUrl.empty() && url == baseUrl) {
        return response;
    }

    // If the url is overridden, and the redirect parameter is not ForceRedirect,
    // the SDK should not redirect the calls, and it just has to return the response.
    if (urlIsCustom && preferences->redirectMode != RedirectMode::Force) {
        return response;
    }

    // The next call should use the preferences url provided in the config json
    url = baseUrl;

    if (preferences->redirectMode == RedirectMode::No) {
        return response;
    }

    // Try to download again with the new url

    if (preferences->redirectMode == RedirectMode::Should) {
        LOG_WARN(3002) <<
            "The `dataGovernance` parameter specified at the client initialization is not in sync with the preferences on the ConfigCat Dashboard. "
            "Read more: https://configcat.com/docs/advanced/data-governance/";
    }

    if (executeCount > 0) {
        return executeFetch(eTag, executeCount - 1);
    }

    LOG_ERROR(1104) << "Redirection loop encountered while trying to fetch config JSON. Please contact us at https://configcat.com/support/";
    return response;
}

FetchResponse ConfigFetcher::fetch(const std::string& eTag) {
    if (!httpSessionAdapter) {
        auto error = "HttpSessionAdapter is not provided.";
        LOG_ERROR(0) << error;
        assert(false);
        return FetchResponse(failure, ConfigEntry::empty, error, true);
    }

    string requestUrl(url + "/configuration-files/" + sdkKey + "/" + kConfigJsonName);
    std::map<std::string, std::string> requestHeader = {
        {kUserAgentHeaderName, userAgent},
        {kPlatformHeaderName, getPlatformName()}
    };
    if (!eTag.empty()) {
        requestHeader.insert({kIfNoneMatchHeaderName, eTag});
    }

    auto response = httpSessionAdapter->get(requestUrl, requestHeader, proxies, proxyAuthentications);
    if (response.operationTimedOut) {
        LogEntry logEntry = LogEntry(logger, LOG_LEVEL_ERROR, 1102);
        logEntry << "Request timed out while trying to fetch config JSON. "
                    "Timeout values: [connect: " << connectTimeoutMs << "ms, read: " << readTimeoutMs << "ms]";
        return FetchResponse(failure, ConfigEntry::empty, logEntry.getMessage(), true);
    }
    if (response.error.length() > 0) {
        LogEntry logEntry(logger, LOG_LEVEL_ERROR, 1103);
        logEntry << "Unexpected error occurred while trying to fetch config JSON: " << response.error;
        return FetchResponse(failure, ConfigEntry::empty, logEntry.getMessage(), true);
    }

    switch (response.statusCode) {
        case 200:
        case 201:
        case 202:
        case 203:
        case 204: {
            const auto it = response.header.find(kEtagHeaderName);
            string eTag = it != response.header.end() ? it->second : "";
            try {
                auto config = Config::fromJson(response.text);
                LOG_DEBUG << "Fetch was successful: new config fetched.";
                return FetchResponse(fetched, make_shared<ConfigEntry>(config, eTag, response.text, getUtcNowSecondsSinceEpoch()));
            } catch (exception& exception) {
                LogEntry logEntry(logger, LOG_LEVEL_ERROR, 1105);
                logEntry <<
                    "Fetching config JSON was successful but the HTTP response content was invalid. "
                    "Config JSON parsing failed. " << exception.what();
                return FetchResponse(failure, ConfigEntry::empty, logEntry.getMessage(), true);
            }
        }

        case 304:
            LOG_DEBUG << "Fetch was successful: config not modified.";
            return FetchResponse({notModified, ConfigEntry::empty});

        case 403:
        case 404: {
            LogEntry logEntry(logger, LOG_LEVEL_ERROR, 1100);
            logEntry <<
                "Your SDK Key seems to be wrong. You can find the valid SDK Key at https://app.configcat.com/sdkkey. "
                "Received unexpected response: " << response.statusCode;
            return FetchResponse(failure, ConfigEntry::empty, logEntry.getMessage(), false);
        }

        default: {
            LogEntry logEntry(logger, LOG_LEVEL_ERROR, 1101);
            logEntry << "Unexpected HTTP response was received while trying to fetch config JSON: " << response.statusCode;
            return FetchResponse(failure, ConfigEntry::empty, logEntry.getMessage(), true);
        }
    }
}

} // namespace configcat
