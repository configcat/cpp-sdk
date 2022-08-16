#pragma once

#include <string>
#include "datagovernance.h"
#include "pollingmode.h"
#include "configcatcache.h"
#include "httpsessionadapter.h"
#include "flagoverrides.h"

namespace configcat {

struct ConfigCatOptions {
    std::string baseUrl = "";
    DataGovernance dataGovernance = Global;
    uint32_t connectTimeout = 8000; // milliseconds (0 means it never times out during transfer)
    uint32_t readTimeout = 5000; // milliseconds (0 means it never times out during transfer)
    std::shared_ptr<PollingMode> mode = PollingMode::autoPoll();
    std::shared_ptr<ConfigCatCache> cache;
    std::shared_ptr<FlagOverrides> override;
    std::shared_ptr<HttpSessionAdapter> httpSessionAdapter;
};

} // namespace configcat
