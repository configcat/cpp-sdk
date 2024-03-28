#pragma once

#include <algorithm>
#include <array>
#include <functional>
#include <iterator>
#include <limits>
#include <chrono>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace configcat {

using date_time_t = std::chrono::system_clock::time_point;

constexpr auto kEpochTime = date_time_t(); // January 1, 1970 UTC

inline double getUtcNowSecondsSinceEpoch() {
    auto duration = std::chrono::system_clock::now() - kEpochTime;
    return std::chrono::duration<double>(duration).count();
}

std::optional<double> datetime_to_unixtimeseconds(const std::chrono::system_clock::time_point& tp);
std::optional<date_time_t> datetime_from_unixtimeseconds(double timestamp);

std::string datetime_to_isostring(const date_time_t& tp);
date_time_t make_datetime(int year, int month, int day, int hour, int min, int sec, int millisec);

} // namespace configcat
