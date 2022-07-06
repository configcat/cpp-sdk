#pragma once

#include <string>
#include <unordered_map>

namespace configcat {

// An object containing attributes to properly identify a given user for rollout evaluation.
class ConfigCatUser {
public:
    ConfigCatUser(const std::string& identifier,
                  const std::string& email = {},
                  const std::string& country = {},
                  const std::unordered_map<std::string, std::string>& custom = {});

    const std::string identifier;
    const std::string email;
    const std::string country;
    const std::unordered_map<std::string, std::string> custom;
};

} // namespace configcat
