#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <vector>
#include <list>
#include "configcat/configcatuser.h"
#include "configcat/configcatclient.h"
#include "configcatlogger.h"
#include "utils.h"
#include "test.h"
#include "mock.h"
#include "configcat/log.h"
#include "configcat/fileoverridedatasource.h"
#include "configcat/mapoverridedatasource.h"
#include "rolloutevaluator.h"

using namespace std;
using namespace configcat;

class ConfigV2EvaluationTest : public ::testing::Test {
public:
    static const string directoryPath;
};
const string ConfigV2EvaluationTest::directoryPath = RemoveFileName(__FILE__);

class ComparisonAttributeConversionToCanonicalStringTestSuite : public ::testing::TestWithParam<tuple<string, ConfigCatUser::AttributeValue, string>> {};
INSTANTIATE_TEST_SUITE_P(ConfigV2EvaluationTest, ComparisonAttributeConversionToCanonicalStringTestSuite, ::testing::Values(
    make_tuple("numberToStringConversion", .12345, "1"),
    make_tuple("numberToStringConversionInt", 125.0, "4"),
    make_tuple("numberToStringConversionPositiveExp", -1.23456789e96, "2"),
    make_tuple("numberToStringConversionNegativeExp", -12345.6789E-100, "4"),
    make_tuple("numberToStringConversionNaN", NAN, "3"),
    make_tuple("numberToStringConversionPositiveInf", INFINITY, "4"),
    make_tuple("numberToStringConversionNegativeInf", -INFINITY, "3"),
    make_tuple("dateToStringConversion", make_datetime(2023, 3, 31, 23, 59, 59, 999), "3"),
    make_tuple("dateToStringConversion", 1680307199.999, "3"), // Assuming this needs conversion to date
    make_tuple("dateToStringConversionNaN", NAN, "3"),
    make_tuple("dateToStringConversionPositiveInf", INFINITY, "1"),
    make_tuple("dateToStringConversionNegativeInf", -INFINITY, "5"),
    make_tuple("stringArrayToStringConversion", vector<string>{ "read", "Write", " eXecute " }, "4"),
    make_tuple("stringArrayToStringConversionEmpty", vector<string>{}, "5"),
    make_tuple("stringArrayToStringConversionSpecialChars", vector<string>{"+<>%\"'\\/\t\r\n"}, "3"),
    make_tuple("stringArrayToStringConversionUnicode", vector<string>{"äöüÄÖÜçéèñışğâ¢™✓😀"}, "2")
));
TEST_P(ComparisonAttributeConversionToCanonicalStringTestSuite, ComparisonAttributeConversionToCanonicalString) {
    auto [key, customAttributeValue, expectedReturnValue] = GetParam();

    ConfigCatOptions options;
    options.pollingMode = PollingMode::manualPoll();
    options.flagOverrides = make_shared<FileFlagOverrides>(ConfigV2EvaluationTest::directoryPath + "data/comparison_attribute_conversion.json", LocalOnly);
    auto client = ConfigCatClient::get("local-only", &options);

    std::unordered_map<string, ConfigCatUser::AttributeValue> custom = { {"Custom1", customAttributeValue} };
    auto user = make_shared<ConfigCatUser>("12345", nullopt, nullopt, custom);

    auto result = client->getValue(key, "default", user);

    EXPECT_EQ(expectedReturnValue, result);

    ConfigCatClient::closeAll();
}

class ComparisonAttributeTrimmingTestSuite : public ::testing::TestWithParam<tuple<string, string>> {};
INSTANTIATE_TEST_SUITE_P(ConfigV2EvaluationTest, ComparisonAttributeTrimmingTestSuite, ::testing::Values(
    make_tuple("isoneof", "no trim"),
    make_tuple("isnotoneof", "no trim"),
    make_tuple("isoneofhashed", "no trim"),
    make_tuple("isnotoneofhashed", "no trim"),
    make_tuple("equalshashed", "no trim"),
    make_tuple("notequalshashed", "no trim"),
    make_tuple("arraycontainsanyofhashed", "no trim"),
    make_tuple("arraynotcontainsanyofhashed", "no trim"),
    make_tuple("equals", "no trim"),
    make_tuple("notequals", "no trim"),
    make_tuple("startwithanyof", "no trim"),
    make_tuple("notstartwithanyof", "no trim"),
    make_tuple("endswithanyof", "no trim"),
    make_tuple("notendswithanyof", "no trim"),
    make_tuple("arraycontainsanyof", "no trim"),
    make_tuple("arraynotcontainsanyof", "no trim"),
    make_tuple("startwithanyofhashed", "no trim"),
    make_tuple("notstartwithanyofhashed", "no trim"),
    make_tuple("endswithanyofhashed", "no trim"),
    make_tuple("notendswithanyofhashed", "no trim"),
    // semver comparators user values trimmed because of backward compatibility
    make_tuple("semverisoneof", "4 trim"),
    make_tuple("semverisnotoneof", "5 trim"),
    make_tuple("semverless", "6 trim"),
    make_tuple("semverlessequals", "7 trim"),
    make_tuple("semvergreater", "8 trim"),
    make_tuple("semvergreaterequals", "9 trim"),
    // number and date comparators user values trimmed because of backward compatibility
    make_tuple("numberequals", "10 trim"),
    make_tuple("numbernotequals", "11 trim"),
    make_tuple("numberless", "12 trim"),
    make_tuple("numberlessequals", "13 trim"),
    make_tuple("numbergreater", "14 trim"),
    make_tuple("numbergreaterequals", "15 trim"),
    make_tuple("datebefore", "18 trim"),
    make_tuple("dateafter", "19 trim"),
    // "contains any of" and "not contains any of" is a special case, the not trimmed user attribute checked against not trimmed comparator values.
    make_tuple("containsanyof", "no trim"),
    make_tuple("notcontainsanyof", "no trim")
));
TEST_P(ComparisonAttributeTrimmingTestSuite, ComparisonAttributeTrimming) {
    auto [key, expectedReturnValue] = GetParam();

    ConfigCatOptions options;
    options.pollingMode = PollingMode::manualPoll();
    options.flagOverrides = make_shared<FileFlagOverrides>(ConfigV2EvaluationTest::directoryPath + "data/comparison_attribute_trimming.json", LocalOnly);
    auto client = ConfigCatClient::get("local-only", &options);

    std::unordered_map<string, ConfigCatUser::AttributeValue> custom = {
        {"Version", " 1.0.0 "},
        {"Number", " 3 "},
        {"Date", " 1705253400 "}
    };
    auto user = make_shared<ConfigCatUser>(" 12345 ", nullopt, "[\" USA \"]", custom);

    auto result = client->getValue(key, "default", user);

    EXPECT_EQ(expectedReturnValue, result);

    ConfigCatClient::closeAll();
}

class ComparisonValueTrimmingTestSuite : public ::testing::TestWithParam<tuple<string, string>> {};
INSTANTIATE_TEST_SUITE_P(ConfigV2EvaluationTest, ComparisonValueTrimmingTestSuite, ::testing::Values(
    make_tuple("isoneof", "no trim"),
    make_tuple("isnotoneof", "no trim"),
    make_tuple("containsanyof", "no trim"),
    make_tuple("notcontainsanyof", "no trim"),
    make_tuple("isoneofhashed", "no trim"),
    make_tuple("isnotoneofhashed", "no trim"),
    make_tuple("equalshashed", "no trim"),
    make_tuple("notequalshashed", "no trim"),
    make_tuple("arraycontainsanyofhashed", "no trim"),
    make_tuple("arraynotcontainsanyofhashed", "no trim"),
    make_tuple("equals", "no trim"),
    make_tuple("notequals", "no trim"),
    make_tuple("startwithanyof", "no trim"),
    make_tuple("notstartwithanyof", "no trim"),
    make_tuple("endswithanyof", "no trim"),
    make_tuple("notendswithanyof", "no trim"),
    make_tuple("arraycontainsanyof", "no trim"),
    make_tuple("arraynotcontainsanyof", "no trim"),
    make_tuple("startwithanyofhashed", "no trim"),
    make_tuple("notstartwithanyofhashed", "no trim"),
    make_tuple("endswithanyofhashed", "no trim"),
    make_tuple("notendswithanyofhashed", "no trim"),
    // semver comparator values trimmed because of backward compatibility
    make_tuple("semverisoneof", "4 trim"),
    make_tuple("semverisnotoneof", "5 trim"),
    make_tuple("semverless", "6 trim"),
    make_tuple("semverlessequals", "7 trim"),
    make_tuple("semvergreater", "8 trim"),
    make_tuple("semvergreaterequals", "9 trim")
));
TEST_P(ComparisonValueTrimmingTestSuite, ComparisonValueTrimming) {
    auto [key, expectedReturnValue] = GetParam();

    ConfigCatOptions options;
    options.pollingMode = PollingMode::manualPoll();
    options.flagOverrides = make_shared<FileFlagOverrides>(ConfigV2EvaluationTest::directoryPath + "data/comparison_value_trimming.json", LocalOnly);
    auto client = ConfigCatClient::get("local-only", &options);

    std::unordered_map<string, ConfigCatUser::AttributeValue> custom = {
        {"Version", "1.0.0"},
        {"Number", "3"},
        {"Date", "1705253400"}
    };
    auto user = make_shared<ConfigCatUser>("12345", nullopt, "[\"USA\"]", custom);

    auto result = client->getValue(key, "default", user);

    EXPECT_EQ(expectedReturnValue, result);

    ConfigCatClient::closeAll();
}

TEST_F(ConfigV2EvaluationTest, UserObjectAttributeValueConversionTextComparisons) {
    std::shared_ptr<TestLogger> testLogger = make_shared<TestLogger>(LOG_LEVEL_WARNING);

    ConfigCatOptions options;
    options.pollingMode = PollingMode::manualPoll();
    options.logger = testLogger;
    auto client = ConfigCatClient::get("configcat-sdk-1/JcPbCGl_1E-K9M-fJOyKyQ/OfQqcTjfFUGBwMKqtyEOrQ", &options);
    client->forceRefresh();

    string key = "boolTextEqualsNumber";
    string customAttributeName = "Custom1";
    int customAttributeValue = 42;

    std::unordered_map<string, ConfigCatUser::AttributeValue> custom = {
        {customAttributeName, customAttributeValue}
    };
    auto user = make_shared<ConfigCatUser>("12345", nullopt, nullopt, custom);

    auto result = client->getValue(key, false, user);
    EXPECT_TRUE(result);

    auto expectedLog = string_format("WARNING [3005] Evaluation of condition (User.%s EQUALS '%s') for setting '%s' may not produce the expected"
        " result (the User.%s attribute is not a string value, thus it was automatically converted to the "
        "string value '%s'). Please make sure that using a non-string value was intended.\n",
        customAttributeName.c_str(), number_to_string(customAttributeValue).c_str(),
        key.c_str(), customAttributeName.c_str(), number_to_string(customAttributeValue).c_str());
    EXPECT_EQ(expectedLog, testLogger->text);

    ConfigCatClient::closeAll();
}

class UserObjectAttributeValueConversion_NonTextComparisonsTestSuite : public ::testing::TestWithParam<tuple<string, string, string, string, ConfigCatUser::AttributeValue, Value>> {};
INSTANTIATE_TEST_SUITE_P(ConfigV2EvaluationTest, UserObjectAttributeValueConversion_NonTextComparisonsTestSuite, ::testing::Values(
    // SemVer-based comparisons
    make_tuple("configcat-sdk-1/PKDVCLf-Hq-h-kCzMp-L7Q/iV8vH2MBakKxkFZylxHmTg", "lessThanWithPercentage", "12345", "Custom1", "0.0", "20%"),
    make_tuple("configcat-sdk-1/PKDVCLf-Hq-h-kCzMp-L7Q/iV8vH2MBakKxkFZylxHmTg", "lessThanWithPercentage", "12345", "Custom1", "0.9.9", "< 1.0.0"),
    make_tuple("configcat-sdk-1/PKDVCLf-Hq-h-kCzMp-L7Q/iV8vH2MBakKxkFZylxHmTg", "lessThanWithPercentage", "12345", "Custom1", "1.0.0", "20%"),
    make_tuple("configcat-sdk-1/PKDVCLf-Hq-h-kCzMp-L7Q/iV8vH2MBakKxkFZylxHmTg", "lessThanWithPercentage", "12345", "Custom1", "1.1", "20%"),
    make_tuple("configcat-sdk-1/PKDVCLf-Hq-h-kCzMp-L7Q/iV8vH2MBakKxkFZylxHmTg", "lessThanWithPercentage", "12345", "Custom1", 0, "20%"),
    make_tuple("configcat-sdk-1/PKDVCLf-Hq-h-kCzMp-L7Q/iV8vH2MBakKxkFZylxHmTg", "lessThanWithPercentage", "12345", "Custom1", 0.9, "20%"),
    make_tuple("configcat-sdk-1/PKDVCLf-Hq-h-kCzMp-L7Q/iV8vH2MBakKxkFZylxHmTg", "lessThanWithPercentage", "12345", "Custom1", 2, "20%"),
    // Number-based comparisons
    make_tuple("configcat-sdk-1/PKDVCLf-Hq-h-kCzMp-L7Q/FCWN-k1dV0iBf8QZrDgjdw", "numberWithPercentage", "12345", "Custom1", -INFINITY, "<2.1"),
    make_tuple("configcat-sdk-1/PKDVCLf-Hq-h-kCzMp-L7Q/FCWN-k1dV0iBf8QZrDgjdw", "numberWithPercentage", "12345", "Custom1", -1, "<2.1"),
    make_tuple("configcat-sdk-1/PKDVCLf-Hq-h-kCzMp-L7Q/FCWN-k1dV0iBf8QZrDgjdw", "numberWithPercentage", "12345", "Custom1", 2, "<2.1"),
    make_tuple("configcat-sdk-1/PKDVCLf-Hq-h-kCzMp-L7Q/FCWN-k1dV0iBf8QZrDgjdw", "numberWithPercentage", "12345", "Custom1", 2.1, "<=2,1"),
    make_tuple("configcat-sdk-1/PKDVCLf-Hq-h-kCzMp-L7Q/FCWN-k1dV0iBf8QZrDgjdw", "numberWithPercentage", "12345", "Custom1", 3, "<>4.2"),
    make_tuple("configcat-sdk-1/PKDVCLf-Hq-h-kCzMp-L7Q/FCWN-k1dV0iBf8QZrDgjdw", "numberWithPercentage", "12345", "Custom1", 5, ">=5"),
    make_tuple("configcat-sdk-1/PKDVCLf-Hq-h-kCzMp-L7Q/FCWN-k1dV0iBf8QZrDgjdw", "numberWithPercentage", "12345", "Custom1", INFINITY, ">5"),
    make_tuple("configcat-sdk-1/PKDVCLf-Hq-h-kCzMp-L7Q/FCWN-k1dV0iBf8QZrDgjdw", "numberWithPercentage", "12345", "Custom1", NAN, "<>4.2"),
    make_tuple("configcat-sdk-1/PKDVCLf-Hq-h-kCzMp-L7Q/FCWN-k1dV0iBf8QZrDgjdw", "numberWithPercentage", "12345", "Custom1", "-Infinity", "<2.1"),
    make_tuple("configcat-sdk-1/PKDVCLf-Hq-h-kCzMp-L7Q/FCWN-k1dV0iBf8QZrDgjdw", "numberWithPercentage", "12345", "Custom1", "-1", "<2.1"),
    make_tuple("configcat-sdk-1/PKDVCLf-Hq-h-kCzMp-L7Q/FCWN-k1dV0iBf8QZrDgjdw", "numberWithPercentage", "12345", "Custom1", "2", "<2.1"),
    make_tuple("configcat-sdk-1/PKDVCLf-Hq-h-kCzMp-L7Q/FCWN-k1dV0iBf8QZrDgjdw", "numberWithPercentage", "12345", "Custom1", "2.1", "<=2,1"),
    make_tuple("configcat-sdk-1/PKDVCLf-Hq-h-kCzMp-L7Q/FCWN-k1dV0iBf8QZrDgjdw", "numberWithPercentage", "12345", "Custom1", "2,1", "<=2,1"),
    make_tuple("configcat-sdk-1/PKDVCLf-Hq-h-kCzMp-L7Q/FCWN-k1dV0iBf8QZrDgjdw", "numberWithPercentage", "12345", "Custom1", "3", "<>4.2"),
    make_tuple("configcat-sdk-1/PKDVCLf-Hq-h-kCzMp-L7Q/FCWN-k1dV0iBf8QZrDgjdw", "numberWithPercentage", "12345", "Custom1", "5", ">=5"),
    make_tuple("configcat-sdk-1/PKDVCLf-Hq-h-kCzMp-L7Q/FCWN-k1dV0iBf8QZrDgjdw", "numberWithPercentage", "12345", "Custom1", "Infinity", ">5"),
    make_tuple("configcat-sdk-1/PKDVCLf-Hq-h-kCzMp-L7Q/FCWN-k1dV0iBf8QZrDgjdw", "numberWithPercentage", "12345", "Custom1", "NaN", "<>4.2"),
    make_tuple("configcat-sdk-1/PKDVCLf-Hq-h-kCzMp-L7Q/FCWN-k1dV0iBf8QZrDgjdw", "numberWithPercentage", "12345", "Custom1", "NaNa", "80%"),
    // Date time-based comparisons
    make_tuple("configcat-sdk-1/JcPbCGl_1E-K9M-fJOyKyQ/OfQqcTjfFUGBwMKqtyEOrQ", "boolTrueIn202304", "12345", "Custom1", make_datetime(2023, 3, 31, 23, 59, 59, 999), false),
    make_tuple("configcat-sdk-1/JcPbCGl_1E-K9M-fJOyKyQ/OfQqcTjfFUGBwMKqtyEOrQ", "boolTrueIn202304", "12345", "Custom1", make_datetime(2023, 4, 1, 0, 0, 0, 1), true),
    make_tuple("configcat-sdk-1/JcPbCGl_1E-K9M-fJOyKyQ/OfQqcTjfFUGBwMKqtyEOrQ", "boolTrueIn202304", "12345", "Custom1", make_datetime(2023, 4, 30, 23, 59, 59, 999), true),
    make_tuple("configcat-sdk-1/JcPbCGl_1E-K9M-fJOyKyQ/OfQqcTjfFUGBwMKqtyEOrQ", "boolTrueIn202304", "12345", "Custom1", make_datetime(2023, 5, 1, 0, 0, 0, 1), false),
    make_tuple("configcat-sdk-1/JcPbCGl_1E-K9M-fJOyKyQ/OfQqcTjfFUGBwMKqtyEOrQ", "boolTrueIn202304", "12345", "Custom1", -INFINITY, false),
    make_tuple("configcat-sdk-1/JcPbCGl_1E-K9M-fJOyKyQ/OfQqcTjfFUGBwMKqtyEOrQ", "boolTrueIn202304", "12345", "Custom1", 1680307199.999, false),
    make_tuple("configcat-sdk-1/JcPbCGl_1E-K9M-fJOyKyQ/OfQqcTjfFUGBwMKqtyEOrQ", "boolTrueIn202304", "12345", "Custom1", 1680307200.001, true),
    make_tuple("configcat-sdk-1/JcPbCGl_1E-K9M-fJOyKyQ/OfQqcTjfFUGBwMKqtyEOrQ", "boolTrueIn202304", "12345", "Custom1", 1682899199.999, true),
    make_tuple("configcat-sdk-1/JcPbCGl_1E-K9M-fJOyKyQ/OfQqcTjfFUGBwMKqtyEOrQ", "boolTrueIn202304", "12345", "Custom1", 1682899200.001, false),
    make_tuple("configcat-sdk-1/JcPbCGl_1E-K9M-fJOyKyQ/OfQqcTjfFUGBwMKqtyEOrQ", "boolTrueIn202304", "12345", "Custom1", INFINITY, false),
    make_tuple("configcat-sdk-1/JcPbCGl_1E-K9M-fJOyKyQ/OfQqcTjfFUGBwMKqtyEOrQ", "boolTrueIn202304", "12345", "Custom1", NAN, false),
    make_tuple("configcat-sdk-1/JcPbCGl_1E-K9M-fJOyKyQ/OfQqcTjfFUGBwMKqtyEOrQ", "boolTrueIn202304", "12345", "Custom1", 1680307199, false),
    make_tuple("configcat-sdk-1/JcPbCGl_1E-K9M-fJOyKyQ/OfQqcTjfFUGBwMKqtyEOrQ", "boolTrueIn202304", "12345", "Custom1", 1680307201, true),
    make_tuple("configcat-sdk-1/JcPbCGl_1E-K9M-fJOyKyQ/OfQqcTjfFUGBwMKqtyEOrQ", "boolTrueIn202304", "12345", "Custom1", 1682899199, true),
    make_tuple("configcat-sdk-1/JcPbCGl_1E-K9M-fJOyKyQ/OfQqcTjfFUGBwMKqtyEOrQ", "boolTrueIn202304", "12345", "Custom1", 1682899201, false),
    make_tuple("configcat-sdk-1/JcPbCGl_1E-K9M-fJOyKyQ/OfQqcTjfFUGBwMKqtyEOrQ", "boolTrueIn202304", "12345", "Custom1", "-Infinity", false),
    make_tuple("configcat-sdk-1/JcPbCGl_1E-K9M-fJOyKyQ/OfQqcTjfFUGBwMKqtyEOrQ", "boolTrueIn202304", "12345", "Custom1", "1680307199.999", false),
    make_tuple("configcat-sdk-1/JcPbCGl_1E-K9M-fJOyKyQ/OfQqcTjfFUGBwMKqtyEOrQ", "boolTrueIn202304", "12345", "Custom1", "1680307200.001", true),
    make_tuple("configcat-sdk-1/JcPbCGl_1E-K9M-fJOyKyQ/OfQqcTjfFUGBwMKqtyEOrQ", "boolTrueIn202304", "12345", "Custom1", "1682899199.999", true),
    make_tuple("configcat-sdk-1/JcPbCGl_1E-K9M-fJOyKyQ/OfQqcTjfFUGBwMKqtyEOrQ", "boolTrueIn202304", "12345", "Custom1", "1682899200.001", false),
    make_tuple("configcat-sdk-1/JcPbCGl_1E-K9M-fJOyKyQ/OfQqcTjfFUGBwMKqtyEOrQ", "boolTrueIn202304", "12345", "Custom1", "+Infinity", false),
    make_tuple("configcat-sdk-1/JcPbCGl_1E-K9M-fJOyKyQ/OfQqcTjfFUGBwMKqtyEOrQ", "boolTrueIn202304", "12345", "Custom1", "NaN", false),
    // String array-based comparisons
    make_tuple("configcat-sdk-1/JcPbCGl_1E-K9M-fJOyKyQ/OfQqcTjfFUGBwMKqtyEOrQ", "stringArrayContainsAnyOfDogDefaultCat", "12345", "Custom1", vector<string>{"x", "read"}, "Dog"),
    make_tuple("configcat-sdk-1/JcPbCGl_1E-K9M-fJOyKyQ/OfQqcTjfFUGBwMKqtyEOrQ", "stringArrayContainsAnyOfDogDefaultCat", "12345", "Custom1", vector<string>{"x", "Read"}, "Cat"),
    make_tuple("configcat-sdk-1/JcPbCGl_1E-K9M-fJOyKyQ/OfQqcTjfFUGBwMKqtyEOrQ", "stringArrayContainsAnyOfDogDefaultCat", "12345", "Custom1", "[\"x\", \"read\"]", "Dog"),
    make_tuple("configcat-sdk-1/JcPbCGl_1E-K9M-fJOyKyQ/OfQqcTjfFUGBwMKqtyEOrQ", "stringArrayContainsAnyOfDogDefaultCat", "12345", "Custom1", "[\"x\", \"Read\"]", "Cat"),
    make_tuple("configcat-sdk-1/JcPbCGl_1E-K9M-fJOyKyQ/OfQqcTjfFUGBwMKqtyEOrQ", "stringArrayContainsAnyOfDogDefaultCat", "12345", "Custom1", "x, read", "Cat")
));
TEST_P(UserObjectAttributeValueConversion_NonTextComparisonsTestSuite, UserObjectAttributeValueConversion_NonTextComparisons) {
    auto [sdkKey, key, userId, customAttributeName, customAttributeValue, expectedReturnValue] = GetParam();

    ConfigCatOptions options;
    options.pollingMode = PollingMode::manualPoll();
    auto client = ConfigCatClient::get(sdkKey, &options);
    client->forceRefresh();

    std::unordered_map<string, ConfigCatUser::AttributeValue> custom = {
        {customAttributeName, customAttributeValue}
    };
    auto user = make_shared<ConfigCatUser>(userId, nullopt, nullopt, custom);

    auto details = client->getValueDetails(key, user);
    EXPECT_EQ(expectedReturnValue, details.value);

    ConfigCatClient::closeAll();
}

class PrerequisiteFlagCircularDependencyTestSuite : public ::testing::TestWithParam<tuple<string, string>> {};
INSTANTIATE_TEST_SUITE_P(ConfigV2EvaluationTest, PrerequisiteFlagCircularDependencyTestSuite, ::testing::Values(
    make_tuple("key1", "'key1' -> 'key1'"),
    make_tuple("key2", "'key2' -> 'key3' -> 'key2'"),
    make_tuple("key4", "'key4' -> 'key3' -> 'key2' -> 'key3'")
));
TEST_P(PrerequisiteFlagCircularDependencyTestSuite, PrerequisiteFlagCircularDependency) {
    auto [key, dependencyCycle] = GetParam();

    std::shared_ptr<TestLogger> testLogger = make_shared<TestLogger>();
    ConfigCatOptions options;
    options.pollingMode = PollingMode::manualPoll();
    options.logger = testLogger;
    options.flagOverrides = make_shared<FileFlagOverrides>(ConfigV2EvaluationTest::directoryPath + "data/test_circulardependency_v6.json", LocalOnly);
    auto client = ConfigCatClient::get("local-only", &options);

    auto details = client->getValueDetails(key);

    EXPECT_TRUE(details.isDefaultValue);
    EXPECT_TRUE(details.value == nullopt);
    EXPECT_TRUE(details.errorMessage != nullopt);
    EXPECT_TRUE(details.errorException != nullptr);
    auto exceptionMessage = unwrap_exception_message(details.errorException);
    EXPECT_THAT(exceptionMessage, ::testing::HasSubstr("Circular dependency detected"));
    EXPECT_THAT(exceptionMessage, ::testing::HasSubstr(dependencyCycle));
    EXPECT_THAT(testLogger->text, ::testing::HasSubstr("Circular dependency detected"));
    EXPECT_THAT(testLogger->text, ::testing::HasSubstr(dependencyCycle));

    ConfigCatClient::closeAll();
}

// https://app.configcat.com/v2/e7a75611-4256-49a5-9320-ce158755e3ba/08dbc325-7f69-4fd4-8af4-cf9f24ec8ac9/08dbc325-9e4e-4f59-86b2-5da50924b6ca/08dbc325-9ebd-4587-8171-88f76a3004cb
class PrerequisiteFlagComparisonValueTypeMismatchTestSuite : public ::testing::TestWithParam<tuple<string, string, Value, std::optional<Value>>> {};
INSTANTIATE_TEST_SUITE_P(ConfigV2EvaluationTest, PrerequisiteFlagComparisonValueTypeMismatchTestSuite, ::testing::Values(
    make_tuple("stringDependsOnBool", "mainBoolFlag", true, "Dog"),
    make_tuple("stringDependsOnBool", "mainBoolFlag", false, "Cat"),
    make_tuple("stringDependsOnBool", "mainBoolFlag", "1", nullopt),
    make_tuple("stringDependsOnBool", "mainBoolFlag", 1, nullopt),
    make_tuple("stringDependsOnBool", "mainBoolFlag", 1.0, nullopt),
    make_tuple("stringDependsOnString", "mainStringFlag", "private", "Dog"),
    make_tuple("stringDependsOnString", "mainStringFlag", "Private", "Cat"),
    make_tuple("stringDependsOnString", "mainStringFlag", true, nullopt),
    make_tuple("stringDependsOnString", "mainStringFlag", 1, nullopt),
    make_tuple("stringDependsOnString", "mainStringFlag", 1.0, nullopt),
    make_tuple("stringDependsOnInt", "mainIntFlag", 2, "Dog"),
    make_tuple("stringDependsOnInt", "mainIntFlag", 1, "Cat"),
    make_tuple("stringDependsOnInt", "mainIntFlag", "2", nullopt),
    make_tuple("stringDependsOnInt", "mainIntFlag", true, nullopt),
    make_tuple("stringDependsOnInt", "mainIntFlag", 2.0, nullopt),
    make_tuple("stringDependsOnDouble", "mainDoubleFlag", 0.1, "Dog"),
    make_tuple("stringDependsOnDouble", "mainDoubleFlag", 0.11, "Cat"),
    make_tuple("stringDependsOnDouble", "mainDoubleFlag", "0.1", nullopt),
    make_tuple("stringDependsOnDouble", "mainDoubleFlag", true, nullopt),
    make_tuple("stringDependsOnDouble", "mainDoubleFlag", 1, nullopt)
));
TEST_P(PrerequisiteFlagComparisonValueTypeMismatchTestSuite, PrerequisiteFlagComparisonValueTypeMismatch) {
    auto [key, prerequisiteFlagKey, prerequisiteFlagValue, expectedReturnValue] = GetParam();

    ConfigCatOptions options;
    options.pollingMode = PollingMode::manualPoll();
    options.flagOverrides = make_shared<MapFlagOverrides>(std::unordered_map<std::string, Value>{{ prerequisiteFlagKey, prerequisiteFlagValue }}, LocalOverRemote);
    auto client = ConfigCatClient::get("configcat-sdk-1/JcPbCGl_1E-K9M-fJOyKyQ/JoGwdqJZQ0K2xDy7LnbyOg", &options);
    client->forceRefresh();

    auto details = client->getValueDetails(key);

    if (expectedReturnValue) {
        EXPECT_FALSE(details.isDefaultValue);
        EXPECT_EQ(expectedReturnValue, details.value);
        EXPECT_TRUE(details.errorMessage == nullopt);
        EXPECT_TRUE(details.errorException == nullptr);
    } else {
        EXPECT_TRUE(details.isDefaultValue);
        EXPECT_TRUE(details.value == nullopt);
        EXPECT_TRUE(details.errorMessage != nullopt);
        EXPECT_TRUE(details.errorException != nullptr);
        auto exceptionMessage = unwrap_exception_message(details.errorException);
        EXPECT_THAT(exceptionMessage, testing::MatchesRegex("Type mismatch between comparison value '[^']+' and prerequisite flag '[^']+'."));
    };

    ConfigCatClient::closeAll();
}

// https://app.configcat.com/v2/e7a75611-4256-49a5-9320-ce158755e3ba/08dbc325-7f69-4fd4-8af4-cf9f24ec8ac9/08dbc325-9e4e-4f59-86b2-5da50924b6ca/08dbc325-9ebd-4587-8171-88f76a3004cb
class PrerequisiteFlagOverrideTestSuite : public ::testing::TestWithParam<tuple<string, string, string, std::optional<OverrideBehaviour>, std::optional<Value>>> {};
INSTANTIATE_TEST_SUITE_P(ConfigV2EvaluationTest, PrerequisiteFlagOverrideTestSuite, ::testing::Values(
    make_tuple("stringDependsOnString", "1", "john@sensitivecompany.com", nullopt, "Dog"),
    make_tuple("stringDependsOnString", "1", "john@sensitivecompany.com", RemoteOverLocal, "Dog"),
    make_tuple("stringDependsOnString", "1", "john@sensitivecompany.com", LocalOverRemote, "Dog"),
    make_tuple("stringDependsOnString", "1", "john@sensitivecompany.com", LocalOnly, nullopt),
    make_tuple("stringDependsOnString", "2", "john@notsensitivecompany.com", nullopt, "Cat"),
    make_tuple("stringDependsOnString", "2", "john@notsensitivecompany.com", RemoteOverLocal, "Cat"),
    make_tuple("stringDependsOnString", "2", "john@notsensitivecompany.com", LocalOverRemote, "Dog"),
    make_tuple("stringDependsOnString", "2", "john@notsensitivecompany.com", LocalOnly, nullopt),
    make_tuple("stringDependsOnInt", "1", "john@sensitivecompany.com", nullopt, "Dog"),
    make_tuple("stringDependsOnInt", "1", "john@sensitivecompany.com", RemoteOverLocal, "Dog"),
    make_tuple("stringDependsOnInt", "1", "john@sensitivecompany.com", LocalOverRemote, "Cat"),
    make_tuple("stringDependsOnInt", "1", "john@sensitivecompany.com", LocalOnly, nullopt),
    make_tuple("stringDependsOnInt", "2", "john@notsensitivecompany.com", nullopt, "Cat"),
    make_tuple("stringDependsOnInt", "2", "john@notsensitivecompany.com", RemoteOverLocal, "Cat"),
    make_tuple("stringDependsOnInt", "2", "john@notsensitivecompany.com", LocalOverRemote, "Dog"),
    make_tuple("stringDependsOnInt", "2", "john@notsensitivecompany.com", LocalOnly, nullopt)
));
TEST_P(PrerequisiteFlagOverrideTestSuite, PrerequisiteFlagOverride) {
    auto [key, userId, email, overrideBehaviour, expectedReturnValue] = GetParam();

    ConfigCatOptions options;
    options.pollingMode = PollingMode::manualPoll();
    if (overrideBehaviour) {
        options.flagOverrides = make_shared<FileFlagOverrides>(ConfigV2EvaluationTest::directoryPath + "data/test_override_flagdependency_v6.json", *overrideBehaviour);
    }
    auto client = ConfigCatClient::get("configcat-sdk-1/JcPbCGl_1E-K9M-fJOyKyQ/JoGwdqJZQ0K2xDy7LnbyOg", &options);
    client->forceRefresh();

    auto user = make_shared<ConfigCatUser>(userId, email);
    auto details = client->getValueDetails(key, user);

    if (expectedReturnValue) {
        EXPECT_FALSE(details.isDefaultValue);
        EXPECT_EQ(expectedReturnValue, details.value);
        EXPECT_TRUE(details.errorMessage == nullopt);
        EXPECT_TRUE(details.errorException == nullptr);
    } else {
        EXPECT_TRUE(details.isDefaultValue);
        EXPECT_TRUE(details.value == nullopt);
        EXPECT_TRUE(details.errorMessage != nullopt);
    };

    ConfigCatClient::closeAll();
}

// https://app.configcat.com/v2/e7a75611-4256-49a5-9320-ce158755e3ba/08dbc325-7f69-4fd4-8af4-cf9f24ec8ac9/08dbc325-9e4e-4f59-86b2-5da50924b6ca/08dbc325-9ebd-4587-8171-88f76a3004cb
class ConfigSaltAndSegmentsOverrideTestSuite : public ::testing::TestWithParam<tuple<string, string, string, std::optional<OverrideBehaviour>, std::optional<Value>>> {};
INSTANTIATE_TEST_SUITE_P(ConfigV2EvaluationTest, ConfigSaltAndSegmentsOverrideTestSuite, ::testing::Values(
    make_tuple("developerAndBetaUserSegment", "1", "john@example.com", nullopt, false),
    make_tuple("developerAndBetaUserSegment", "1", "john@example.com", RemoteOverLocal, false),
    make_tuple("developerAndBetaUserSegment", "1", "john@example.com", LocalOverRemote, true),
    make_tuple("developerAndBetaUserSegment", "1", "john@example.com", LocalOnly, true),
    make_tuple("notDeveloperAndNotBetaUserSegment", "2", "kate@example.com", nullopt, true),
    make_tuple("notDeveloperAndNotBetaUserSegment", "2", "kate@example.com", RemoteOverLocal, true),
    make_tuple("notDeveloperAndNotBetaUserSegment", "2", "kate@example.com", LocalOverRemote, true),
    make_tuple("notDeveloperAndNotBetaUserSegment", "2", "kate@example.com", LocalOnly, nullopt)
));
TEST_P(ConfigSaltAndSegmentsOverrideTestSuite, ConfigSaltAndSegmentsOverride) {
    auto [key, userId, email, overrideBehaviour, expectedReturnValue] = GetParam();

    ConfigCatOptions options;
    options.pollingMode = PollingMode::manualPoll();
    if (overrideBehaviour) {
        options.flagOverrides = make_shared<FileFlagOverrides>(ConfigV2EvaluationTest::directoryPath + "data/test_override_segments_v6.json", *overrideBehaviour);
    }
    auto client = ConfigCatClient::get("configcat-sdk-1/JcPbCGl_1E-K9M-fJOyKyQ/h99HYXWWNE2bH8eWyLAVMA", &options);
    client->forceRefresh();

    auto user = make_shared<ConfigCatUser>(userId, email);
    auto details = client->getValueDetails(key, user);

    if (expectedReturnValue) {
        EXPECT_FALSE(details.isDefaultValue);
        EXPECT_EQ(expectedReturnValue, details.value);
        EXPECT_TRUE(details.errorMessage == nullopt);
        EXPECT_TRUE(details.errorException == nullptr);
    } else {
        EXPECT_TRUE(details.isDefaultValue);
        EXPECT_TRUE(details.value == nullopt);
        EXPECT_TRUE(details.errorMessage != nullopt);
    };

    ConfigCatClient::closeAll();
}

// https://app.configcat.com/v2/e7a75611-4256-49a5-9320-ce158755e3ba/08dbc325-7f69-4fd4-8af4-cf9f24ec8ac9/08dbc325-9e4e-4f59-86b2-5da50924b6ca/08dbc325-9ebd-4587-8171-88f76a3004cb
class EvaluationDetailsMatchedEvaluationRuleAndPercentageOptionTestSuite : public ::testing::TestWithParam<tuple<string, string, std::optional<string>, std::optional<string>, std::optional<string>, std::optional<Value>, bool, bool>> {};
INSTANTIATE_TEST_SUITE_P(ConfigV2EvaluationTest, EvaluationDetailsMatchedEvaluationRuleAndPercentageOptionTestSuite, ::testing::Values(
    make_tuple("configcat-sdk-1/JcPbCGl_1E-K9M-fJOyKyQ/P4e3fAz_1ky2-Zg2e4cbkw", "stringMatchedTargetingRuleAndOrPercentageOption", nullopt, nullopt, nullopt, "Cat", false, false),
    make_tuple("configcat-sdk-1/JcPbCGl_1E-K9M-fJOyKyQ/P4e3fAz_1ky2-Zg2e4cbkw", "stringMatchedTargetingRuleAndOrPercentageOption", "12345", nullopt, nullopt, "Cat", false, false),
    make_tuple("configcat-sdk-1/JcPbCGl_1E-K9M-fJOyKyQ/P4e3fAz_1ky2-Zg2e4cbkw", "stringMatchedTargetingRuleAndOrPercentageOption", "12345", "a@example.com", nullopt, "Dog", true, false),
    make_tuple("configcat-sdk-1/JcPbCGl_1E-K9M-fJOyKyQ/P4e3fAz_1ky2-Zg2e4cbkw", "stringMatchedTargetingRuleAndOrPercentageOption", "12345", "a@configcat.com", nullopt, "Cat", false, false),
    make_tuple("configcat-sdk-1/JcPbCGl_1E-K9M-fJOyKyQ/P4e3fAz_1ky2-Zg2e4cbkw", "stringMatchedTargetingRuleAndOrPercentageOption", "12345", "a@configcat.com", "", "Frog", true, true),
    make_tuple("configcat-sdk-1/JcPbCGl_1E-K9M-fJOyKyQ/P4e3fAz_1ky2-Zg2e4cbkw", "stringMatchedTargetingRuleAndOrPercentageOption", "12345", "a@configcat.com", "US", "Fish", true, true),
    make_tuple("configcat-sdk-1/JcPbCGl_1E-K9M-fJOyKyQ/P4e3fAz_1ky2-Zg2e4cbkw", "stringMatchedTargetingRuleAndOrPercentageOption", "12345", "b@configcat.com", nullopt, "Cat", false, false),
    make_tuple("configcat-sdk-1/JcPbCGl_1E-K9M-fJOyKyQ/P4e3fAz_1ky2-Zg2e4cbkw", "stringMatchedTargetingRuleAndOrPercentageOption", "12345", "b@configcat.com", "", "Falcon", false, true),
    make_tuple("configcat-sdk-1/JcPbCGl_1E-K9M-fJOyKyQ/P4e3fAz_1ky2-Zg2e4cbkw", "stringMatchedTargetingRuleAndOrPercentageOption", "12345", "b@configcat.com", "US", "Spider", false, true)
));
TEST_P(EvaluationDetailsMatchedEvaluationRuleAndPercentageOptionTestSuite, EvaluationDetailsMatchedEvaluationRuleAndPercentageOption) {
    auto [sdkKey, key, userId, email, percentageBase, expectedReturnValue, expectedMatchedTargetingRuleSet, expectedMatchedPercentageOptionSet] = GetParam();

    ConfigCatOptions options;
    options.pollingMode = PollingMode::manualPoll();
    auto client = ConfigCatClient::get(sdkKey, &options);
    client->forceRefresh();


    std::unordered_map<string, ConfigCatUser::AttributeValue> custom = {};
    if (percentageBase)
        custom.insert({"PercentageBase", *percentageBase});
    auto user = (userId) ? make_shared<ConfigCatUser>(*userId, *email, nullopt, custom) : nullptr;
    auto details = client->getValueDetails(key, user);

    EXPECT_EQ(expectedReturnValue, details.value);
    EXPECT_EQ(expectedMatchedTargetingRuleSet, details.matchedTargetingRule != nullopt);
    EXPECT_EQ(expectedMatchedPercentageOptionSet, details.matchedPercentageOption != nullopt);

    ConfigCatClient::closeAll();
}
