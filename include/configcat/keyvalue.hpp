#pragma once

#include <string>
#include <unordered_map>
#include <variant>

namespace configcat {

using Value = std::variant<int, double, std::string>;

struct KeyValue {
    std::string key;
    Value value;
};

} // namespace configcat
