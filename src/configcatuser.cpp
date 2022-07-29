#include "configcat/configcatuser.h"
#include <nlohmann/json.hpp>

using namespace std;
using json = nlohmann::json;

namespace configcat {

ConfigCatUser::ConfigCatUser(const string& id,
                             const string& email,
                             const string& country,
                             const unordered_map<string, string>& custom):
    identifier(attributes["Identifier"]) {
    attributes["Identifier"] = id;
    if (!email.empty()) attributes["Email"] = email;
    if (!country.empty()) attributes["Country"] = country;
    attributes.insert(custom.begin(), custom.end());
}

const string* ConfigCatUser::getAttribute(const string& key) const {
    auto it = attributes.find(key);
    if (it != attributes.end()) {
        return &it->second;
    }

    return nullptr;
}

string ConfigCatUser::toJson() const {
    json user(attributes);
    return user.dump(4);
}

} // namespace configcat
