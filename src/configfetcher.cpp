#include "configfetcher.h"

#include <cpr/cpr.h>
#include "configcat/log.h"
#include "configcat/httpsessionadapter.h"
#include "configcat/configcatoptions.h"
#include "configcat/configcatlogger.h"
#include "version.h"
#include "platform.h"
#include "utils.h"

using namespace std;

namespace configcat {

class SessionInterceptor : public cpr::Interceptor {
public:
    SessionInterceptor(shared_ptr<HttpSessionAdapter> httpSessionAdapter):
        httpSessionAdapter(httpSessionAdapter) {
    }

    virtual cpr::Response intercept(cpr::Session& session) {
        configcat::Response adapterResponse = httpSessionAdapter->get(session.GetFullRequestUrl(), header);
        header.clear();
        cpr::Response response;
        response.status_code = adapterResponse.status_code;
        response.text = adapterResponse.text;
        for (auto keyValue : adapterResponse.header) {
            response.header.insert({keyValue.first, keyValue.second});
        }
        return response;
    }

    void setHeader(const cpr::Header& requestHeader) {
        for (auto keyValue : requestHeader) {
            header.insert({keyValue.first, keyValue.second});
        }
    }

    void close() {
        httpSessionAdapter->close();
    }

private:
    shared_ptr<HttpSessionAdapter> httpSessionAdapter;
    std::map<std::string, std::string> header;
};

ConfigFetcher::ConfigFetcher(const string& sdkKey, shared_ptr<ConfigCatLogger> logger, const string& mode, const ConfigCatOptions& options):
    sdkKey(sdkKey),
    logger(logger),
    mode(mode),
    connectTimeoutMs(options.connectTimeoutMs),
    readTimeoutMs(options.readTimeoutMs) {
    if (options.httpSessionAdapter) {
        sessionInterceptor = make_shared<SessionInterceptor>(options.httpSessionAdapter);
    }
    session = make_unique<cpr::Session>();
    session->SetConnectTimeout(connectTimeoutMs);
    session->SetTimeout(readTimeoutMs);
    session->SetProxies(options.proxies);
    std::map<std::string, cpr::EncodedAuthentication> proxyAuthentications;
    for (auto& keyValue : options.proxyAuthentications) {
        auto& authentications = keyValue.second;
        proxyAuthentications.insert({keyValue.first, {authentications.user, authentications.password}});
    }
    session->SetProxyAuth(proxyAuthentications);
    session->SetProgressCallback(cpr::ProgressCallback{
            [&](size_t /*downloadTotal*/, size_t /*downloadNow*/, size_t /*uploadTotal*/, size_t /*uploadNow*/,
                intptr_t /*userdata*/) -> bool {
                return !closed;
            }
    });
    urlIsCustom = !options.baseUrl.empty();
    url = urlIsCustom
        ? options.baseUrl
        : options.dataGovernance == DataGovernance::Global
            ? kGlobalBaseUrl
            : kEuOnlyBaseUrl;
}

ConfigFetcher::~ConfigFetcher() {
}

void ConfigFetcher::close() {
    if (sessionInterceptor) {
        sessionInterceptor->close();
    }
    closed = true;
}

FetchResponse ConfigFetcher::fetchConfiguration(const std::string& eTag) {
    return executeFetch(eTag, 2);
}

FetchResponse ConfigFetcher::executeFetch(const std::string& eTag, int executeCount) {
    auto response = fetch(eTag);
    auto preferences = response.entry && response.entry->config ? response.entry->config->preferences : nullptr;

    // If there wasn't a config change or there were no preferences in the config, we return the response
    if (!response.isFetched() || preferences == nullptr) {
        return response;
    }

    // If the preferences url is the same as the last called one, just return the response.
    if (!preferences->url.empty() && url == preferences->url) {
        return response;
    }

    // If the url is overridden, and the redirect parameter is not ForceRedirect,
    // the SDK should not redirect the calls, and it just has to return the response.
    if (urlIsCustom && preferences->redirect != ForceRedirect) {
        return response;
    }

    // The next call should use the preferences url provided in the config json
    url = preferences->url;

    if (preferences->redirect == NoRedirect) {
        return response;
    }

    // Try to download again with the new url

    if (preferences->redirect == ShouldRedirect) {
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
    cpr::Header header = {
        {kUserAgentHeaderName, string("ConfigCat-Cpp/") + mode + "-" + CONFIGCAT_VERSION},
        {kPlatformHeaderName, getPlatformName()}
    };
    if (!eTag.empty()) {
        header.insert({kIfNoneMatchHeaderName, eTag});
    }
    session->SetHeader(header);
    session->SetUrl(url + "/configuration-files/" + sdkKey + "/" + kConfigJsonName);

    // The session's interceptor will be unregistered after the cpr::Session::Get() call.
    // We always register it before the call.
    if (sessionInterceptor) {
        sessionInterceptor->setHeader(header);
        session->AddInterceptor(sessionInterceptor);
    }

    auto response = session->Get();

    if (response.error.code != cpr::ErrorCode::OK) {
        LogEntry logEntry = response.error.code == cpr::ErrorCode::OPERATION_TIMEDOUT
            ? LogEntry(logger, LOG_LEVEL_ERROR, 1102) <<
                "Request timed out while trying to fetch config JSON. "
                "Timeout values: [connect: " << connectTimeoutMs << "ms, read: " << readTimeoutMs << "ms]"
            : LogEntry(logger, LOG_LEVEL_ERROR, 1103) <<
                "Unexpected error occurred while trying to fetch config JSON: " << response.error.message;

        return FetchResponse(failure, ConfigEntry::empty, logEntry.getMessage(), true);
    }

    switch (response.status_code) {
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
                return FetchResponse(fetched, make_shared<ConfigEntry>(config, eTag, getUtcNowSecondsSinceEpoch()));
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
                "Received unexpected response: " << response.status_code;
            return FetchResponse(failure, ConfigEntry::empty, logEntry.getMessage(), false);
        }

        default: {
            LogEntry logEntry(logger, LOG_LEVEL_ERROR, 1101);
            logEntry << "Unexpected HTTP response was received while trying to fetch config JSON: " << response.status_code;
            return FetchResponse(failure, ConfigEntry::empty, logEntry.getMessage(), true);
        }
    }
}

} // namespace configcat
