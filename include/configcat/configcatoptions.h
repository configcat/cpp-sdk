#pragma once

#include <string>

#include "datagovernance.h"
#include "pollingmode.h"

namespace configcat {

struct ConfigCatOptions {
    std::string baseUrl = "";
    DataGovernance dataGovernance = Global;
    uint32_t connectTimeout = 8000; // milliseconds
    uint32_t readTimeout = 5000; // milliseconds
    std::shared_ptr<PollingMode> mode = PollingMode::autoPoll();
//    ConfigCatCache? cache;
//    FlagOverrides? override;
//    HttpClientAdapter? httpClientAdapter;
};

} // namespace configcat
