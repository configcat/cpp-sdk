#include "configfetcher.h"

#include <cpr/cpr.h>
#include "configcat/log.h"
#include "configcat/httpsessionadapter.h"
#include "configcat/configcatoptions.h"
#include "configentry.h"
#include "version.h"
#include "platform.h"

using namespace std;

namespace configcat {

class SessionInterceptor : public cpr::Interceptor {
public:
    SessionInterceptor(shared_ptr<HttpSessionAdapter> httpSessionAdapter):
        httpSessionAdapter(httpSessionAdapter) {
    }

    virtual cpr::Response intercept(cpr::Session& session) {
        configcat::Response adapterResponse = httpSessionAdapter->get(session.GetFullRequestUrl());
        cpr::Response response;
        response.status_code = adapterResponse.status_code;
        response.text = adapterResponse.text;
        return response;
    }

    void close() {
        httpSessionAdapter->close();
    }

private:
    shared_ptr<HttpSessionAdapter> httpSessionAdapter;
};

ConfigFetcher::ConfigFetcher(const string& sdkKey, const string& mode, const ConfigCatOptions& options):
    sdkKey(sdkKey),
    mode(mode) {
    if (options.httpSessionAdapter) {
        sessionInterceptor = make_shared<SessionInterceptor>(options.httpSessionAdapter);
    }
    session = make_unique<cpr::Session>();
    session->SetConnectTimeout(options.connectTimeout);
    session->SetTimeout(options.readTimeout);
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
        LOG_WARN << "Your dataGovernance parameter at ConfigCatClient "
                    "initialization is not in sync with your preferences on the ConfigCat "
                    "Dashboard: https://app.configcat.com/organization/data-governance. "
                    "Only Organization Admins can access this preference.";
    }

    if (executeCount > 0) {
        return executeFetch(eTag, executeCount - 1);
    }

    LOG_ERROR << "Redirect loop during config.json fetch. Please contact support@configcat.com.";
    return response;
}

FetchResponse ConfigFetcher::fetch(const std::string& eTag) {
    cpr::Header header = {
        {kUserAgentHeaderName, string("ConfigCat-cpp-") + getPlatformName() + "/" + mode + "-" + CONFIGCAT_VERSION}
    };
    if (!eTag.empty()) {
        header.insert({kIfNoneMatchHeaderName, eTag});
    }
    session->SetHeader(header);
    session->SetUrl(url + "/configuration-files/" + sdkKey + "/" + kConfigJsonName);

    // The session's interceptor will be unregistered after the cpr::Session::Get() call.
    // We always register it before the call.
    if (sessionInterceptor) {
        session->AddInterceptor(sessionInterceptor);
    }

    auto response = session->Get();

    switch (response.status_code) {
        case 200:
        case 201:
        case 202:
        case 203:
        case 204: {
            const auto it = response.header.find(kEtagHeaderName);
            string eTag = it != response.header.end() ? it->second : "";
            auto& jsonString = response.text;
            auto config = Config::fromJson(jsonString, eTag);
            if (config == Config::empty) {
                return FetchResponse({failure, ConfigEntry::empty});
            }

            LOG_DEBUG << "Fetch was successful: new config fetched.";
            return FetchResponse({fetched, make_shared<ConfigEntry>(jsonString, config, eTag, std::chrono::steady_clock::now())});
        }

        case 304:
            LOG_DEBUG << "Fetch was successful: config not modified.";
            return FetchResponse({notModified, ConfigEntry::empty});

        default:
            LOG_ERROR << "Double-check your API KEY at https://app.configcat.com/apikey. " <<
                "Received unexpected response: " << response.status_code;
            return FetchResponse({failure, ConfigEntry::empty});
    }
}

} // namespace configcat
