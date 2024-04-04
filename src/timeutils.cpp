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

// https://howardhinnant.github.io/date_algorithms.html#days_from_civil
// From C++20, this can be replaced with std::chrono::sys_days
template <class Int> constexpr Int days_from_civil(Int y, unsigned m, unsigned d) noexcept {
    static_assert(std::numeric_limits<unsigned>::digits >= 18, "This algorithm has not been ported to a 16 bit unsigned integer");
    static_assert(std::numeric_limits<Int>::digits >= 20, "This algorithm has not been ported to a 16 bit signed integer");
    y -= m <= 2;
    const Int era = (y >= 0 ? y : y-399) / 400;
    const unsigned yoe = static_cast<unsigned>(y - era * 400);  // [0, 399]
    const unsigned doy = (153*(m > 2 ? m-3 : m+9) + 2)/5 + d-1; // [0, 365]
    const unsigned doe = yoe * 365 + yoe/4 - yoe/100 + doy;     // [0, 146096]
    return era * 146097 + static_cast<Int>(doe) - 719468;
}

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
    auto days = days_from_civil(static_cast<int32_t>(year), month, day);
    constexpr auto dayInSeconds = 86400;
    auto duration = std::chrono::seconds(static_cast<long long>(days) * dayInSeconds + sec) +
                    std::chrono::hours(hour) +
                    std::chrono::minutes(min) +
                    std::chrono::milliseconds(millisec);
    return date_time_t{duration};
}

} // namespace configcat
