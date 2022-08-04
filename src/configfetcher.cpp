#include "configcat/configfetcher.h"

#include <cpr/cpr.h>
#include "configcat/log.h"
#include "configcat/httpsessionadapter.h"
#include "configcat/configcatoptions.h"
#include "configcat/configjsoncache.h"
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

private:
    shared_ptr<HttpSessionAdapter> httpSessionAdapter;
};

ConfigFetcher::ConfigFetcher(const string& sdkKey, const string& mode, ConfigJsonCache& jsonCache, const ConfigCatOptions& options):
    sdkKey(sdkKey),
    mode(mode),
    jsonCache(jsonCache) {
    if (options.httpSessionAdapter) {
        sessionInterceptor = make_shared<SessionInterceptor>(options.httpSessionAdapter);
    }
    session = make_unique<cpr::Session>();
    session->SetConnectTimeout(options.connectTimeout);
    session->SetTimeout(options.readTimeout);
    urlIsCustom = !options.baseUrl.empty();
    url = urlIsCustom
        ? options.baseUrl
        : options.dataGovernance == DataGovernance::Global
            ? kGlobalBaseUrl
            : kEuOnlyBaseUrl;
}

ConfigFetcher::~ConfigFetcher() {
}

FetchResponse ConfigFetcher::fetchConfiguration() {
    return executeFetch(2);
}

FetchResponse ConfigFetcher::executeFetch(int executeCount) {
    auto response = fetch();
    auto preferences = response.config ? response.config->preferences : nullptr;

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
        return executeFetch(executeCount - 1);
    }

    LOG_ERROR << "Redirect loop during config.json fetch. Please contact support@configcat.com.";
    return response;
}

FetchResponse ConfigFetcher::fetch() {
    auto cache = jsonCache.readCache();
    cpr::Header header = {
        {kUserAgentHeaderName, string("ConfigCat-cpp-") + getPlatformName() + "/" + mode + "-" + CONFIGCAT_VERSION}
    };
    if (!cache->eTag.empty()) {
        header.insert({kIfNoneMatchHeaderName, cache->eTag});
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
            auto config = jsonCache.readFromJson(response.text, eTag);
            if (config == Config::empty) {
                return FetchResponse({failure, Config::empty});
            }

            LOG_DEBUG << "Fetch was successful: new config fetched.";
            return FetchResponse({fetched, config});
        }

        case 304:
            LOG_DEBUG << "Fetch was successful: config not modified.";
            return FetchResponse({notModified, Config::empty});

        default:
            LOG_ERROR << "Double-check your API KEY at https://app.configcat.com/apikey. " <<
                "Received unexpected response: " << response.status_code;
            return FetchResponse({failure, Config::empty});
    }
}

} // namespace configcat
