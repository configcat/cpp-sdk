#include <iostream>
#include <cpr/cpr.h>
#include <rapidjson/document.h>
#include "configcat/log.h"
#include "configcat/httpsessionadapter.h"
#include "configcat/configcatoptions.h"
#include "configcat/configfetcher.h"
#include "configcat/configjsoncache.h"

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

ConfigFetcher::ConfigFetcher(const std::string& sdkKey, const std::string& mode, ConfigJsonCache& jsonCache, const ConfigCatOptions& options) {
    const string url("https://cdn-global.configcat.com/configuration-files/PKDVCLf-Hq-h-kCzMp-L7Q/psuH7BGHoUmdONrzzUOY7A/config_v5.json");
    cpr::Session session;
    session.SetUrl(url);
    if (options.httpSessionAdapter) {
        session.AddInterceptor(make_shared<SessionInterceptor>(options.httpSessionAdapter));
    }
    cpr::Response response = session.Get();

    if (response.status_code == 200) {
        LOG_INFO << "Got successful response from " << url;
        LOG_INFO << response.text;

        code = response.status_code;
        data = response.text;

        rapidjson::Document json;
        json.Parse(data.c_str());
    } else {
        LOG_ERROR << "Couldn't GET from " << url << " - exiting";
    }
}


} // namespace configcat
