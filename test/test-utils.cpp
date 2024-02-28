#include <cmath>
#include <tuple>
#include <gtest/gtest.h>
#include "utils.h"

using namespace configcat;
using namespace std;

TEST(UtilsTest, string_format_test_1) {
    auto s = string_format("");

    ASSERT_EQ("", s);
    ASSERT_EQ(0, (int)s[s.length()]);
}

TEST(UtilsTest, string_format_test_2) {
    auto s = string_format("", "x");

    ASSERT_EQ("", s);
    ASSERT_EQ(0, (int)s[s.length()]);
}

TEST(UtilsTest, string_format_test_3) {
    string format(STRING_FORMAT_STACKBUF_MAXSIZE - 1, 'a');
    auto s = string_format(format);

    ASSERT_EQ(format, s);
    ASSERT_EQ(0, (int)s[s.length()]);
}

TEST(UtilsTest, string_format_test_4) {
    string format(STRING_FORMAT_STACKBUF_MAXSIZE, 'a');
    auto s = string_format(format);

    ASSERT_EQ(format, s);
    ASSERT_EQ(0, (int)s[s.length()]);
}

TEST(UtilsTest, string_format_test_5) {
    auto s = string_format("a%sc", "b");

    ASSERT_EQ("abc", s);
    ASSERT_EQ(0, (int)s[s.length()]);
}

TEST(UtilsTest, string_format_test_6) {
    auto s1 = string(STRING_FORMAT_STACKBUF_MAXSIZE, 'a');
    auto s2 = string(STRING_FORMAT_STACKBUF_MAXSIZE, 'b');
    auto s = string_format("%s-%s", s1.c_str(), s2.c_str());

    ASSERT_EQ(s1 + '-' + s2, s);
    ASSERT_EQ(0, (int)s[s.length()]);
}

TEST(UtilsTest, trim_test_1) {
    string s(" \t\r abc \n");
    trim(s);

    ASSERT_EQ("abc", s);
}

TEST(UtilsTest, formatDateTimeISOTest) {
    auto s = formatDateTimeISO(*dateTimeFromUnixTimeSeconds(0));

    ASSERT_EQ(string("1970-01-01T00:00:00.000Z"), s);
}

class numberFromStringTestSuite : public ::testing::TestWithParam<std::tuple<std::string, std::optional<double>>> {
};

TEST_P(numberFromStringTestSuite, numberFromStringTest) {
    auto [input, expectedOutput] = GetParam();
    auto actualOutput = numberFromString(input);

    ASSERT_EQ(expectedOutput.has_value(), actualOutput.has_value());
    if (expectedOutput.has_value()) {
        if (!isnan(*expectedOutput)) {
            ASSERT_EQ(*expectedOutput, *actualOutput);
        }
        else {
            ASSERT_TRUE(isnan(*actualOutput));
        }
    }
}

INSTANTIATE_TEST_SUITE_P(
    UtilsTest,
    numberFromStringTestSuite,
    ::testing::Values(
        std::make_tuple("", nullopt),
        std::make_tuple(" ", nullopt),
        std::make_tuple("NaN", NAN),
        std::make_tuple("Infinity", INFINITY),
        std::make_tuple("+Infinity", INFINITY),
        std::make_tuple("-Infinity", -INFINITY),
        std::make_tuple("1", 1),
        std::make_tuple("1 ", 1),
        std::make_tuple(" 1", 1),
        std::make_tuple(" 1 ", 1),
        std::make_tuple("0x1", nullopt),
        std::make_tuple(" 0x1", nullopt),
        std::make_tuple("+0x1", nullopt),
        std::make_tuple("-0x1", nullopt),
        std::make_tuple("1f", nullopt),
        std::make_tuple("1e", nullopt),
        std::make_tuple("0+", nullopt),
        std::make_tuple("0-", nullopt),
        std::make_tuple("2023.11.13", nullopt),
        std::make_tuple("0", 0),
        std::make_tuple("-0", 0),
        std::make_tuple("+0", 0),
        std::make_tuple("1234567890", 1234567890),
        std::make_tuple("1234567890.0", 1234567890),
        std::make_tuple("1234567890e0", 1234567890),
        std::make_tuple(".1234567890", 0.1234567890),
        std::make_tuple("+.1234567890", 0.1234567890),
        std::make_tuple("-.1234567890", -0.1234567890),
        std::make_tuple("+0.123e-3", 0.000123),
        std::make_tuple("-0.123e+3", -123)
    ));

class numberToStringTestSuite : public ::testing::TestWithParam<std::tuple<double, std::string>> {
};

TEST_P(numberToStringTestSuite, numberToStringTest) {
    auto [input, expectedOutput] = GetParam();
    auto actualOutput = numberToString(input);

    ASSERT_EQ(expectedOutput, actualOutput);
}

INSTANTIATE_TEST_SUITE_P(
    UtilsTest,
    numberToStringTestSuite,
    ::testing::Values(
        std::make_tuple(NAN, "NaN"),
        std::make_tuple(INFINITY, "Infinity"),
        std::make_tuple(-INFINITY, "-Infinity"),
        std::make_tuple(0, "0"),
        std::make_tuple(1, "1"),
        std::make_tuple(-1, "-1"),
        std::make_tuple(0.1, "0.1"),
        std::make_tuple(-0.1, "-0.1"),
        std::make_tuple(1e-6, "0.000001"),
        std::make_tuple(-1e-6, "-0.000001"),
        std::make_tuple(0.99e-6, "9.9e-7"),
        std::make_tuple(-0.99e-6, "-9.9e-7"),
        std::make_tuple(0.99e21, "990000000000000000000"),
        std::make_tuple(-0.99e21, "-990000000000000000000"),
        std::make_tuple(1e21, "1e+21"),
        std::make_tuple(-1e21, "-1e+21"),
        std::make_tuple(1.000000000000000056e-01, "0.1"),
        std::make_tuple(1.199999999999999956e+00, "1.2"),
        std::make_tuple(1.229999999999999982e+00, "1.23"),
        std::make_tuple(1.233999999999999986e+00, "1.234"),
        std::make_tuple(1.234499999999999931e+00, "1.2345"),
        std::make_tuple(1.002000000000000028e+02, "100.2"),
        std::make_tuple(1.030000000000000000e+05, "103000"),
        std::make_tuple(1.003001000000000005e+02, "100.3001"),
        std::make_tuple(-1.000000000000000056e-01, "-0.1"),
        std::make_tuple(-1.199999999999999956e+00, "-1.2"),
        std::make_tuple(-1.229999999999999982e+00, "-1.23"),
        std::make_tuple(-1.233999999999999986e+00, "-1.234"),
        std::make_tuple(-1.234499999999999931e+00, "-1.2345"),
        std::make_tuple(-1.002000000000000028e+02, "-100.2"),
        std::make_tuple(-1.030000000000000000e+05, "-103000"),
        std::make_tuple(-1.003001000000000005e+02, "-100.3001")
    ));
