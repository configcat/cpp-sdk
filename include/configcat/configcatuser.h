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

    std::string toString() const {
        std::string user("{\n");
        user += "    \"Identifier\": \"" + identifier + "\"";
        if (!email.empty())
            user += ",\n    \"Email\": \"" + email + "\"";
        if (!country.empty())
            user += ",\n    \"Country\": \"" + country + "\"";
        if (!custom.empty()) {
            user += ",\n    \"Custom\": {";
            bool first = true;
            for (const auto& field : custom) {
                user += first ? "\n" : ",\n";
                user += "        \"" + field.first + "\": \"" + field.second + "\"";
                first = false;
            }
            user += "\n    }";
        }
        user += "\n}";
        return user;
    }
};

} // namespace configcat
