#include "configcat/configcatclient.hpp"
#include "configcat/configcatuser.hpp"
#include "configcat/log.hpp"

namespace configcat {

template<>
bool ConfigCatClient::getValue(const std::string& key, const bool& defaultValue, const ConfigCatUser* user) const {
    LOG_DEBUG << "getValue bool";
    return true;
}

template<>
std::string ConfigCatClient::getValue(const std::string& key, const std::string& defaultValue, const ConfigCatUser* user) const {
    return getValue(key, defaultValue.c_str(), user);
}

template<>
int ConfigCatClient::getValue(const std::string& key, const int& defaultValue, const ConfigCatUser* user) const {
    return 42;
}

template<>
double ConfigCatClient::getValue(const std::string& key, const double& defaultValue, const ConfigCatUser* user) const {
    return 42.3;
}

std::string ConfigCatClient::getValue(const std::string& key, const char* defaultValue, const ConfigCatUser* user) const {
    return "string";
}


} // namespace configcat

