#include "configcat/configfetcher.h"

#include <cpr/cpr.h>
#include "configcat/log.h"
#include "configcat/httpsessionadapter.h"
#include "configcat/configcatoptions.h"
#include "configcat/configjsoncache.h"
#include "configcat/version.h"
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
    session = make_unique<cpr::Session>();
    session->SetConnectTimeout(options.connectTimeout);
    session->SetTimeout(options.readTimeout);
    if (options.httpSessionAdapter) {
        session->AddInterceptor(make_shared<SessionInterceptor>(options.httpSessionAdapter));
    }

    url = !options.baseUrl.empty()
        ? options.baseUrl
        : options.dataGovernance == DataGovernance::Global
            ? kGlobalBaseUrl
            : kEuOnlyBaseUrl;
}

ConfigFetcher::~ConfigFetcher() {
}

FetchResponse ConfigFetcher::fetchConfiguration() {
    // TODO: data governance
    return executeFetch(2);
}

FetchResponse ConfigFetcher::executeFetch(int executeCount) {
    auto cache = jsonCache.readCache();
    cpr::Header header = {
        {kUserAgentHeaderName, string("ConfigCat-cpp-") + getPlatformName() + "/" + mode + "-" + CONFIGCAT_VERSION}
    };
    if (!cache->eTag.empty()) {
        header.insert({kIfNoneMatchHeaderName, cache->eTag});
    }
    session->SetHeader(header);
    session->SetUrl(url + "/configuration-files/" + sdkKey + "/" + kConfigJsonName);
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
