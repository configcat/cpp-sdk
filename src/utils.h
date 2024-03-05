#pragma once

#include <algorithm>
#include <array>
#include <stdexcept>
#include <functional>
#include <limits>
#include <chrono>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#define STRING_FORMAT_STACKBUF_MAXSIZE 128

// https://stackoverflow.com/a/76675119/8656352
template<class>
inline constexpr bool always_false_v = false;

namespace configcat {

constexpr auto kEpochTime = std::chrono::system_clock::time_point(); // January 1, 1970 UTC

inline double getUtcNowSecondsSinceEpoch() {
    auto duration = std::chrono::system_clock::now() - kEpochTime;
    return std::chrono::duration<double>(duration).count();
}

std::optional<double> dateTimeToUnixTimeSeconds(const std::chrono::system_clock::time_point& tp);
std::optional<std::chrono::system_clock::time_point> dateTimeFromUnixTimeSeconds(double timestamp);

std::string formatDateTimeISO(const std::chrono::system_clock::time_point& tp);

template<typename... Args>
std::string string_format(const std::string& format, Args&&... args) {
    std::array<char, STRING_FORMAT_STACKBUF_MAXSIZE> stackBuf;
    int size_s = std::snprintf(&stackBuf[0], stackBuf.size(), format.c_str(), std::forward<Args>(args)...);
    if (size_s < 0) {
        throw std::runtime_error("Error during string formatting.");
    }
    else if (size_s < stackBuf.size()) {
        return std::string(&stackBuf[0], &stackBuf[0] + size_s);
    }

    std::string result(
        size_s, // ctor adds the terminating NULL (see https://stackoverflow.com/a/42511919/8656352)
        '\0'); 

    std::snprintf(&result[0], size_s + 1, format.c_str(), std::forward<Args>(args)...);
    return result;
}

template<typename StreamType>
StreamType& appendStringList(
    StreamType& stream,
    const std::vector<std::string>& items,
    size_t maxLength = 0,
    const std::optional<std::function<std::string (size_t)>>& getOmittedItemsText = std::nullopt,
    const char* separator = ", "
) {
    if (!items.empty()) {
        size_t i = 0;
        size_t n = maxLength > 0 && items.size() > maxLength ? maxLength : items.size();
        const char* currentSeparator = "";

        for (const auto& item : items) {
            stream << currentSeparator << "'" << item << "'";
            currentSeparator = separator;

            if (++i >= n) break;
        }

        if (getOmittedItemsText && n < items.size()) {
            stream << (*getOmittedItemsText)(items.size() - maxLength);
        }
    }
    return stream;
}

// trim from start (in place)
inline void ltrim(std::string& s) {
    s.erase(
        s.begin(),
        std::find_if(s.begin(), s.end(), [](unsigned char ch) { return !std::isspace(ch); })
    );
}

// trim from end (in place)
inline void rtrim(std::string& s) {
    s.erase(
        std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) { return !std::isspace(ch); }).base(),
        s.end()
    );
}

// trim from left & right
inline void trim(std::string& s) {
    rtrim(s), ltrim(s);
}

inline bool starts_with(const std::string& str, const std::string& cmp) {
    return str.rfind(cmp, 0) == 0;
}

inline bool ends_with(const std::string& str, const std::string& cmp) {
    const auto maybe_index = str.size() - cmp.size();
    return maybe_index > 0 && (str.find(cmp, maybe_index) == maybe_index);
}

inline bool contains(const std::string& str, const std::string& sub) {
    return str.find(sub) != std::string::npos;
}

std::string numberToString(double number);

std::optional<double> numberFromString(const std::string& str);

std::optional<long long> integerFromString(const std::string& str);

} // namespace configcat
