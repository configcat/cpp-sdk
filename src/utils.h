#pragma once

#include <string>
#include <algorithm>
#include <chrono>

namespace configcat {

constexpr auto kEpochTime = std::chrono::system_clock::time_point(); // January 1, 1970 UTC

inline double getUtcNowSecondsSinceEpoch() {
    auto duration = std::chrono::system_clock::now() - kEpochTime;
    return std::chrono::duration<double>(duration).count();
}

template<typename... Args>
inline std::string string_format(const std::string& format, Args... args) {
    int size_s = std::snprintf(nullptr, 0, format.c_str(), args...) + 1; // Extra space for '\0'
    if (size_s <= 0) {
        throw std::runtime_error("Error during string formatting.");
    }
    auto size = static_cast<size_t>(size_s);
    std::unique_ptr<char[]> buf(new char[size]);
    std::snprintf(buf.get(), size, format.c_str(), args...);
    return std::string(buf.get(), buf.get() + size - 1); // Skip '\0' inside
}

inline std::string str_tolower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
    return s;
}

// trim from left
inline std::string& ltrim(std::string& s, const char* t = " \t\n\r\f\v") {
    s.erase(0, s.find_first_not_of(t));
    return s;
}

// trim from right
inline std::string& rtrim(std::string& s, const char* t = " \t\n\r\f\v") {
    s.erase(s.find_last_not_of(t) + 1);
    return s;
}

// trim from left & right
inline std::string& trim(std::string& s, const char* t = " \t\n\r\f\v") {
    return ltrim(rtrim(s, t), t);
}

inline double str_to_double(const std::string& str, bool& error) {
    char* end = nullptr; // error handler for strtod
    double value;
    if (str.find(",") != std::string::npos) {
        std::string strCopy = str;
        replace(strCopy.begin(), strCopy.end(), ',', '.');
        value = strtod(strCopy.c_str(), &end);
    } else {
        value = strtod(str.c_str(), &end);
    }

    // Handle number conversion error
    error = (*end) ? true : false;

    return value;
}

inline bool starts_with(const std::string& str, const std::string& cmp) {
    return str.compare(0, cmp.length(), cmp) == 0;
}

inline bool contains(const std::string& str, const std::string& sub) {
    return str.find(sub) != std::string::npos;
}

} // namespace configcat
