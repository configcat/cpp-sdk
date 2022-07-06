#include "configcat/configcatuser.hpp"

namespace configcat {

ConfigCatUser::ConfigCatUser(const std::string& identifier,
                             const std::string& email,
                             const std::string& country,
                             const std::unordered_map<std::string, std::string>& custom):
    identifier(identifier),
    email(email),
    country(country),
    custom(custom) {
}

} // namespace configcat
