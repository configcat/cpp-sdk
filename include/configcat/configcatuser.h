#pragma once

#include <string>
#include <unordered_map>

namespace configcat {

// An object containing attributes to properly identify a given user for rollout evaluation.
class ConfigCatUser {
public:
    ConfigCatUser(const std::string& id,
                  const std::string& email = {},
                  const std::string& country = {},
                  const std::unordered_map<std::string, std::string>& custom = {});

    const std::string* getAttribute(const std::string& key) const;
    std::string toJson() const;

private:
    std::unordered_map<std::string, std::string> attributes;

public:
    const std::string& identifier;
};

} // namespace configcat
