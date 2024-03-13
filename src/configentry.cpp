#include <cmath>
#include <stdexcept>

#include "configcat/config.h"
#include "configcat/log.h"
#include "configentry.h"

using namespace std;

namespace configcat {

const shared_ptr<const ConfigEntry> ConfigEntry::empty = make_shared<ConfigEntry>(Config::empty, "empty");

shared_ptr<const ConfigEntry> ConfigEntry::fromString(const string& text) {
    if (text.empty())
        return ConfigEntry::empty;

    auto fetchTimeIndex = text.find('\n');
    auto eTagIndex = text.find('\n', fetchTimeIndex + 1);
    if (fetchTimeIndex == string::npos || eTagIndex == string::npos) {
        throw invalid_argument("Number of values is fewer than expected.");
    }

    auto fetchTimeString = text.substr(0, fetchTimeIndex);
    double fetchTime;
    try {
        fetchTime = stod(fetchTimeString);
    } catch (...) {
        throw invalid_argument("Invalid fetch time: " + fetchTimeString + ". " + unwrap_exception_message(current_exception()));
    }

    auto eTag = text.substr(fetchTimeIndex + 1, eTagIndex - fetchTimeIndex - 1);
    if (eTag.empty()) {
        throw invalid_argument("Empty eTag value");
    }

    auto configJsonString = text.substr(eTagIndex + 1);
    try {
        return make_shared<ConfigEntry>(Config::fromJson(configJsonString), eTag, configJsonString, fetchTime / 1000.0);
    } catch (...) {
        throw invalid_argument("Invalid config JSON: " + configJsonString + ". " + unwrap_exception_message(current_exception()));
    }
}

string ConfigEntry::serialize() const {
    return to_string(static_cast<uint64_t>(floor(fetchTime * 1000))) + "\n" + eTag + "\n" + configJsonString;
}

} // namespace configcat
