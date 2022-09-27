#pragma once

#include <string>
#include <map>
#include "datagovernance.h"
#include "pollingmode.h"
#include "configcatcache.h"
#include "httpsessionadapter.h"
#include "flagoverrides.h"

namespace configcat {

struct ProxyAuthentication {
    std::string user;
    std::string password;
};

struct ConfigCatOptions {
    std::string baseUrl = "";
    DataGovernance dataGovernance = Global;
    uint32_t connectTimeoutMs = 8000; // milliseconds (0 means it never times out during transfer)
    uint32_t readTimeoutMs = 5000; // milliseconds (0 means it never times out during transfer)
    std::shared_ptr<PollingMode> mode = PollingMode::autoPoll();
    std::shared_ptr<ConfigCatCache> cache;
    std::shared_ptr<FlagOverrides> override;
    std::map<std::string, std::string> proxies; // Protocol, Proxy url
    std::map<std::string, ProxyAuthentication> proxyAuthentications; // Protocol, ProxyAuthentication
    std::shared_ptr<HttpSessionAdapter> httpSessionAdapter;
};

} // namespace configcat
