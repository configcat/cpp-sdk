#include "configcat/configcatuser.h"
#include <nlohmann/json.hpp>


using namespace std;
using ordered_json = nlohmann::ordered_json;

namespace configcat {

const ConfigCatUser::AttributeValue* ConfigCatUser::getAttribute(const string& key) const {
    if (key == ConfigCatUser::kIdentifierAttribute) {
        return &identifier;
    }
    if (key == ConfigCatUser::kEmailAttribute) {
        return email ? &*email : nullptr;
    }
    if (key == ConfigCatUser::kCountryAttribute) {
        return country ? &*country : nullptr;
    }
    if (auto it = custom.find(key); it != custom.end()) {
        return &it->second;
    }
    return nullptr;
}

string ConfigCatUser::toJson() const {
    ordered_json j = {
        { kIdentifierAttribute, get<string>(identifier) }
    };

    if (email) j[kEmailAttribute] = get<string>(*email);
    if (country) j[kCountryAttribute] = get<string>(*country);

    for (const auto& [name, setting] : custom) {
        if (name != kIdentifierAttribute && name != kEmailAttribute && name != kCountryAttribute) {
            visit([&j, &nameRef = name] (auto&& alt) { // rebind reference to keep clang compiler happy (https://stackoverflow.com/a/74376436)
                using T = decay_t<decltype(alt)>;
                if constexpr (is_same_v<T, date_time_t>) {
                    j[nameRef] = datetime_to_isostring(alt);
                } else {
                    j[nameRef] = alt;
                }
            }, setting);
        }
    }

    return j.dump();
}

} // namespace configcat
