#include "configcat/rolloutevaluator.h"
#include "configcat/configcatuser.h"
#include "configcat/log.h"
#include "list"

using namespace std;

namespace configcat {

static std::tuple<Value, std::string> evaluate(const Setting& setting, const string& key, const ConfigCatUser* user) {
    LogEntry logEntry(LOG_LEVEL_INFO);
    logEntry << "Evaluating getValue(" << key << ")";

    if (!user) {
        if (!setting.rolloutRules.empty() || !setting.percentageItems.empty()) {
            LOG_WARN << "UserObject missing! You should pass a UserObject to getValue(), "
                     << "in order to make targeting work properly. "
                     << "Read more: https://configcat.com/docs/advanced/user-object/";
            logEntry << "Returning" << setting.value;
            return {setting.value, setting.variationId};
        }
    }

    logEntry << "User object: " << user;



    return {};

    // TODO: evaluation
//    const string* valuePtr = get_if<string>(&setting->second.value);
//    if (valuePtr)
//        return *valuePtr;
}

} // namespace configcat
