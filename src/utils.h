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

#define STRING_FORMAT_STACKBUF_MAXSIZE 128

// https://stackoverflow.com/a/76675119/8656352
template<class>
inline constexpr bool always_false_v = false;

namespace configcat {

template<typename... Args>
std::string string_format(const std::string& format, Args&&... args) {
    std::array<char, STRING_FORMAT_STACKBUF_MAXSIZE> stackBuf;
    int size_s = std::snprintf(&stackBuf[0], stackBuf.size(), format.c_str(), std::forward<Args>(args)...);
    if (size_s < 0) {
        throw std::runtime_error("Error during string formatting.");
    } else if (size_s < stackBuf.size()) {
        return std::string(&stackBuf[0], &stackBuf[0] + size_s);
    }

    std::string result(
        size_s, // ctor adds the terminating NULL (see https://stackoverflow.com/a/42511919/8656352)
        '\0'); 

    std::snprintf(&result[0], size_s + 1, format.c_str(), std::forward<Args>(args)...);
    return result;
}

template<typename StreamType>
StreamType& append_stringlist(
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

inline std::string to_lower(const std::string& str) {
    std::string lowerStr;
    lowerStr.reserve(str.size());
    std::transform(str.begin(), str.end(), std::back_inserter(lowerStr), [](unsigned char c) { return std::tolower(c); });
    return lowerStr;
}

template<typename Type>
inline typename Type::const_iterator findCaseInsensitive(const Type& map, const std::string& searchKey) {
    auto lowerSearchKey = to_lower(searchKey);
    return std::find_if(map.begin(), map.end(), [&lowerSearchKey](const auto& pair) {
        return to_lower(pair.first) == lowerSearchKey;
    });
}

std::string number_to_string(double number);

std::optional<double> number_from_string(const std::string& str);

std::optional<long long> integer_from_string(const std::string& str);

} // namespace configcat
