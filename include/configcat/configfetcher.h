#pragma once

#include <string>
#include <iostream>
#include <cpr/cpr.h>
#include <rapidjson/document.h>
#include "log.h"
#include "httpsessionadapter.h"

namespace configcat {

class SessionInterceptor : public cpr::Interceptor {
public:
    SessionInterceptor(std::shared_ptr<HttpSessionAdapter> httpSessionAdapter):
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
    std::shared_ptr<HttpSessionAdapter> httpSessionAdapter;
};

class ConfigFetcher {
public:
    ConfigFetcher(std::shared_ptr<HttpSessionAdapter> httpSessionAdapter) {
        const std::string url("https://cdn-global.configcat.com/configuration-files/PKDVCLf-Hq-h-kCzMp-L7Q/psuH7BGHoUmdONrzzUOY7A/config_v5.json");
        cpr::Session session;
        session.SetUrl(url);
        if (httpSessionAdapter) {
            session.AddInterceptor(std::make_shared<SessionInterceptor>(httpSessionAdapter));
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

    long code = 0;
    std::string data = "";

private:
};

} // namespace configcat
