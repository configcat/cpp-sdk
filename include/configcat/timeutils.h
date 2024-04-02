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

std::string datetime_to_isostring(const date_time_t& tp);
date_time_t make_datetime(int year, int month, int day, int hour, int min, int sec, int millisec = 0);

} // namespace configcat
