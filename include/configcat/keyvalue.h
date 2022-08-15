#pragma once

#include <string>
#include "config.h"

namespace configcat {

struct KeyValue {
    KeyValue(const std::string& key, const Value& value):
        key(key),
        value(value) {
    }

    std::string key;
    Value value;
};

} // namespace configcat
