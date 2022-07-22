#pragma once

#include <string>
#include "config.h"

namespace configcat {

struct KeyValue {
    std::string key;
    Value value;
};

} // namespace configcat
