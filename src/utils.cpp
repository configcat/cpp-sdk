#include <cmath>

#include "utils.h"

#ifndef CONFIGCAT_EXTERNAL_SHA
#include <hash-library/sha1.h>
#include <hash-library/sha256.h>
#endif // CONFIGCAT_EXTERNAL_SHA

// https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Number/EPSILON
#define JS_NUMBER_EPSILON 2.2204460492503130808472633361816e-16

using namespace std;

namespace configcat {

std::optional<double> datetime_to_unixtimeseconds(const std::chrono::system_clock::time_point& tp) {
    long long millisecondsSinceEpoch = tp.time_since_epoch() / std::chrono::milliseconds(1);
    auto timestamp = millisecondsSinceEpoch / 1000.0;

    // Allow values only between 0001-01-01T00:00:00.000Z and 9999-12-31T23:59:59.999
    return timestamp < -62135596800 || 253402300800 <= timestamp ? nullopt : optional<double>(timestamp);
}

std::optional<std::chrono::system_clock::time_point> datetime_from_unixtimeseconds(double timestamp) {
    // Allow values only between 0001-01-01T00:00:00.000Z and 9999-12-31T23:59:59.999
    if (timestamp < -62135596800 || 253402300800 <= timestamp) {
        return nullopt;
    }

    auto durationSinceEpoch = std::chrono::seconds(static_cast<long long>(timestamp)) +
                              std::chrono::milliseconds(static_cast<long long>((timestamp - static_cast<long long>(timestamp)) * 1000));

    return chrono::system_clock::time_point{ durationSinceEpoch };
}

int getExponent(double abs) {
    auto exp = log10(abs);
    auto ceil = std::ceil(exp);
    return (int)(std::abs(exp - ceil) < JS_NUMBER_EPSILON ? ceil : floor(exp));
}

int getSignificantDecimals(double number) {
    if (!number) {
        return 0;
    }

    number = std::abs(number);
    auto exp = std::min(0, getExponent(number));

    for (; exp > -17; --exp) {
        auto pow10 = pow(10, exp);
        auto fracr = round(number / pow10) * pow10;
        if (abs(number - fracr) < number * 10.0 * JS_NUMBER_EPSILON) {
            break;
        }
    }

    return min(17, -exp);
}

std::string number_to_string(double number) {
    if (isnan(number)) {
        return "NaN";
    }
    else if (isinf(number)) {
        return number > 0 ? "Infinity" : "-Infinity";
    }
    else if (!number) {
        return "0";
    }

    const auto abs = std::abs(number);
    int exp;
    if (1e-6 <= abs && abs < 1e21) {
        exp = 0;
    }
    else {
        exp = getExponent(abs);
        number /= pow(10, exp);
    }

    // NOTE: sprintf can't really deal with 17 decimal places,
    // e.g. sprintf(buf, "%.17f", 0.1) results in '0.10000000000000001'.
    // So we need to manually calculate the actual number of significant decimals.
    auto decimals = getSignificantDecimals(number);

    auto str = string_format(string_format("%%.%df", decimals), number);
    if (exp) {
        str += (exp > 0 ? "e+" : "e") + string_format("%d", exp);
    }

    return str;
}

std::optional<double> number_from_string(const std::string& str) {
    if (str.empty()) return nullopt;

    auto strPtr = const_cast<string*>(&str);

    // Make a copy of str only if necessary.
    string strCopy;
    if (isspace(str[0]) || isspace(str[str.size() - 1])) {
        strCopy = string(str);
        trim(strCopy);
        if (strCopy.empty()) return nullopt;
        strPtr = &strCopy;
    }

    if (*strPtr == "NaN") return numeric_limits<double>::quiet_NaN();
    else if (*strPtr == "Infinity" || *strPtr == "+Infinity") return numeric_limits<double>::infinity();
    else if (*strPtr == "-Infinity") return -numeric_limits<double>::infinity();

    // Accept ',' as decimal separator.
    if (strPtr->find(',') != string::npos) {
        if (strPtr == &str) {
            strCopy = string(str);
            strPtr = &strCopy;
        }

        replace(strPtr->begin(), strPtr->end(), ',', '.');
    }

    // Reject hex numbers and other forms of INF, NAN, etc. that are accepted by std::stod.
    size_t index = 0;
    auto ch = (*strPtr)[index];
    if (ch == '+' || ch == '-') {
        ++index;
        if (index >= strPtr->size()) return nullopt;
        ch = (*strPtr)[index];
    }

    if (isdigit(ch)) {
        ++index;
        if (index < strPtr->size() && !isdigit(ch = (*strPtr)[index]) && ch != '.' && ch != 'e' && ch != 'E') return std::nullopt;
    }
    else if (ch != '.') return nullopt;

    size_t charsProcessed;
    double value;
    try { value = stod(*strPtr, &charsProcessed); }
    catch (const invalid_argument&) { return nullopt; }

    // Reject strings which contain invalid characters after the number.
    if (charsProcessed != strPtr->size()) {
        return nullopt;
    }

    return value;
}

std::optional<long long> integer_from_string(const std::string& str) {
    if (str.empty()) return nullopt;

    auto strPtr = const_cast<string*>(&str);

    // Make a copy of str only if necessary.
    string strCopy;
    if (isspace(str[0]) || isspace(str[str.size() - 1])) {
        strCopy = string(str);
        trim(strCopy);
        if (strCopy.empty()) {
            return nullopt;
        }
        strPtr = &strCopy;
    }

    size_t charsProcessed;
    long long value;
    try { value = stoll(*strPtr, &charsProcessed); }
    catch (const invalid_argument&) { return nullopt; }

    // Reject strings which contain invalid characters after the number.
    if (charsProcessed != strPtr->size()) {
        return nullopt;
    }

    return value;
}

#ifndef CONFIGCAT_EXTERNAL_SHA
SHA1 sha1Calculator;
SHA256 sha256Calculator;

std::string sha1(const std::string& input) {
    return sha1Calculator(input);
}

std::string sha256(const std::string& input) {
    return sha256Calculator(input);
}
#endif // CONFIGCAT_EXTERNAL_SHA


} // namespace configcat
