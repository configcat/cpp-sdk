#include <time.h>

#include "configcat/timeutils.h"

// Even though gmtime_s is part of the C++17 according to the reference (https://en.cppreference.com/w/c/chrono/gmtime),
// yet it's unavailable on some platforms. So, in those cases we need to "polyfill" it. This can be achieved using
// the following SFINAE trick (see also https://slashvar.github.io/2019/08/17/Detecting-Functions-existence-with-SFINAE.html):

template <typename T>
auto gmtime_impl(T tm, const time_t* time, int) -> decltype(gmtime_s(tm, time)) {
    return gmtime_s(tm, time);
}

template <typename T>
struct tm* gmtime_impl(T tm, const time_t* time, float) {
    auto pBuf = gmtime(time);
    *tm = *pBuf;
    return nullptr;
}

using namespace std;

namespace configcat {

std::string datetime_to_isostring(const date_time_t& tp) {
    const auto secondsSinceEpoch = chrono::system_clock::to_time_t(tp);
    const auto remainder = (tp - chrono::system_clock::from_time_t(secondsSinceEpoch));
    const auto milliseconds = chrono::duration_cast<chrono::milliseconds>(remainder).count();

    struct tm time;
    gmtime_impl(&time, &secondsSinceEpoch, 0);

    string result(
        sizeof "1970-01-01T00:00:00.000Z" - 1, // ctor adds the terminating NULL (see https://stackoverflow.com/a/42511919/8656352)
        '\0');
    char* buf = &result[0];
    char* p = buf + strftime(buf, result.size() + 1, "%FT%T", &time);
    snprintf(p, 6, ".%03dZ", static_cast<int>(milliseconds));

    return result;
}

date_time_t make_datetime(int year, int month, int day, int hour, int min, int sec, int millisec) {
    std::tm tm = {};
    tm.tm_year = year - 1900; // Year since 1900
    tm.tm_mon = month - 1;    // 0-11
    tm.tm_mday = day;         // 1-31
    tm.tm_hour = hour;        // 0-23
    tm.tm_min = min;          // 0-59
    tm.tm_sec = sec;          // 0-59

    std::time_t t = std::mktime(&tm);
    auto tp = std::chrono::system_clock::from_time_t(t);
    tp += std::chrono::milliseconds(millisec); // Add milliseconds

    // Correct for the timezone difference since mktime assumes local time
    std::time_t current_time;
    std::time(&current_time);
    auto local_time = std::localtime(&current_time);
    auto utc_time = std::gmtime(&current_time);
    auto diff = std::difftime(std::mktime(local_time), std::mktime(utc_time));
    return tp + chrono::seconds(static_cast<int>(diff));
}

} // namespace configcat
