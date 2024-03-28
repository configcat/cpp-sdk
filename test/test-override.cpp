#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "configcat/mapoverridedatasource.h"
#include "configcat/fileoverridedatasource.h"
#include "configcat/configcatclient.h"
#include "configcatlogger.h"
#include "configcat/consolelogger.h"
#include "mock.h"
#include "test.h"
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;
using namespace configcat;
using namespace std;


template <class T>
class OverrideTestBase : public T {
public:
    void TearDown() {
        for (auto& tempFile : tempFiles) {
            remove(tempFile.c_str());
        }
    }

    // Returns temporary file path if the creation was successful, empty string otherwise
    string createTemporaryFile(const string& content) {
        string tempDir = std::filesystem::temp_directory_path().u8string();
        string file_template = tempDir + "/configcat.XXXXXXXX";
        vector<char> filePath(file_template.begin(), file_template.end());
        filePath.push_back('\0');
        if (!mktemp(filePath.data())) {
            LOG_ERROR(0) << "Cannot create temp filePath.";
            return {};
        }

        std::ofstream file(filePath.data());
        file << content;
        file.close();
        tempFiles.push_back(filePath.data());
        return filePath.data();
    }

    string directoryPath = RemoveFileName(__FILE__);
    vector<string> tempFiles;
    static constexpr char kTestSdkKey[] = "TestSdkKey-23456789012/1234567890123456789012";
    static constexpr char kTestJsonFormat[] = R"({"f":{"fakeKey":{"t":%d,"v":%s}}})";
    ConfigCatClient* client = nullptr;
    shared_ptr<MockHttpSessionAdapter> mockHttpSessionAdapter = make_shared<MockHttpSessionAdapter>();
    shared_ptr<ConfigCatLogger> logger = make_shared<ConfigCatLogger>(make_shared<ConsoleLogger>(), make_shared<Hooks>());
};

class OverrideTest : public OverrideTestBase<testing::Test> {};

TEST_F(OverrideTest, Map) {
    const std::unordered_map<std::string, Value>& map = {
        { "enabledFeature", true },
        { "disabledFeature", false },
        { "intSetting", 5 },
        { "doubleSetting", 3.14 },
        { "stringSetting", "test" }
    };

    ConfigCatOptions options;
    options.pollingMode = PollingMode::manualPoll();
    options.flagOverrides = make_shared<MapFlagOverrides>(map, LocalOnly);
    auto client = ConfigCatClient::get(kTestSdkKey, &options);

    EXPECT_TRUE(client->getValue("enabledFeature", false));
    EXPECT_FALSE(client->getValue("disabledFeature", true));
    EXPECT_EQ(5, client->getValue("intSetting", 0));
    EXPECT_EQ(3.14, client->getValue("doubleSetting", 0.0));
    EXPECT_EQ("test", client->getValue("stringSetting", ""));

    ConfigCatClient::closeAll();
}

TEST_F(OverrideTest, LocalOverRemote) {
    configcat::Response response = {200, string_format(kTestJsonFormat, SettingType::Boolean, R"({"b":false})")};
    mockHttpSessionAdapter->enqueueResponse(response);

    const std::unordered_map<std::string, Value>& map = {
        { "fakeKey", true },
        { "nonexisting", true }
    };

    ConfigCatOptions options;
    options.pollingMode = PollingMode::manualPoll();
    options.httpSessionAdapter = mockHttpSessionAdapter;
    options.flagOverrides = make_shared<MapFlagOverrides>(map, LocalOverRemote);
    auto client = ConfigCatClient::get(kTestSdkKey, &options);
    client->forceRefresh();

    EXPECT_TRUE(client->getValue("fakeKey", false));
    EXPECT_TRUE(client->getValue("nonexisting", false));

    ConfigCatClient::closeAll();
}

TEST_F(OverrideTest, RemoteOverLocal) {
    configcat::Response response = {200, string_format(kTestJsonFormat, SettingType::Boolean, R"({"b":false})")};
    mockHttpSessionAdapter->enqueueResponse(response);

    const std::unordered_map<std::string, Value>& map = {
        { "fakeKey", true },
        { "nonexisting", true }
    };

    ConfigCatOptions options;
    options.pollingMode = PollingMode::manualPoll();
    options.httpSessionAdapter = mockHttpSessionAdapter;
    options.flagOverrides = make_shared<MapFlagOverrides>(map, RemoteOverLocal);
    auto client = ConfigCatClient::get(kTestSdkKey, &options);
    client->forceRefresh();

    EXPECT_FALSE(client->getValue("fakeKey", true));
    EXPECT_TRUE(client->getValue("nonexisting", false));

    ConfigCatClient::closeAll();
}

TEST_F(OverrideTest, File) {
    ConfigCatOptions options;
    options.pollingMode = PollingMode::manualPoll();
    options.flagOverrides = make_shared<FileFlagOverrides>(directoryPath + "/data/test.json", LocalOnly);
    auto client = ConfigCatClient::get(kTestSdkKey, &options);

    EXPECT_TRUE(client->getValue("enabledFeature", false));
    EXPECT_FALSE(client->getValue("disabledFeature", true));
    EXPECT_EQ(5, client->getValue("intSetting", 0));
    EXPECT_EQ(3.14, client->getValue("doubleSetting", 0.0));
    EXPECT_EQ("test", client->getValue("stringSetting", ""));

    ConfigCatClient::closeAll();
}

TEST_F(OverrideTest, SimpleFile) {
    ConfigCatOptions options;
    options.pollingMode = PollingMode::manualPoll();
    options.flagOverrides = make_shared<FileFlagOverrides>(directoryPath + "/data/test-simple.json", LocalOnly);
    auto client = ConfigCatClient::get(kTestSdkKey, &options);

    EXPECT_TRUE(client->getValue("enabledFeature", false));
    EXPECT_FALSE(client->getValue("disabledFeature", true));
    EXPECT_EQ(5, client->getValue("intSetting", 0));
    EXPECT_EQ(3.14, client->getValue("doubleSetting", 0.0));
    EXPECT_EQ("test", client->getValue("stringSetting", ""));

    ConfigCatClient::closeAll();
}

TEST_F(OverrideTest, NonExistentFile) {
    ConfigCatOptions options;
    options.pollingMode = PollingMode::manualPoll();
    options.flagOverrides = make_shared<FileFlagOverrides>(directoryPath + "/data/non-existent.json", LocalOnly);
    auto client = ConfigCatClient::get(kTestSdkKey, &options);

    EXPECT_FALSE(client->getValue("enabledFeature", false));

    ConfigCatClient::closeAll();
}

TEST_F(OverrideTest, ReloadFile) {
    auto filePath = createTemporaryFile(R"({ "flags": { "enabledFeature": false } })");

    // Backdate file modification time so that it will be different when we
    // rewrite it below. This avoids having to add a sleep to the test.
    auto time = std::filesystem::last_write_time(filePath);
    std::filesystem::last_write_time(filePath, time - 1000ms);

    ConfigCatOptions options;
    options.pollingMode = PollingMode::manualPoll();
    options.flagOverrides = make_shared<FileFlagOverrides>(filePath, LocalOnly);
    auto client = ConfigCatClient::get(kTestSdkKey, &options);

    EXPECT_FALSE(client->getValue("enabledFeature", true));

    // Change the temporary file
    std::ofstream file(filePath, ofstream::trunc);
    file << R"({ "flags": { "enabledFeature": true } })";
    file.close();

    EXPECT_TRUE(client->getValue("enabledFeature", false));

    ConfigCatClient::closeAll();
}

template<typename ValueType>
void checkTypeMismatch(const ConfigCatClient& client,
                       std::shared_ptr<TestLogger>& logger,
                       const std::string& key,
                       const string& overrideValueJson,
                       const ValueType& defaultValue,
                       SettingType defaultValueSettingType,
                       const Value& expectedReturnValue) {
    logger->clear();
    EvaluationDetails<ValueType> details = client.getValueDetails(key, defaultValue);

    auto overrideValue = json::parse(overrideValueJson);
    if (overrideValue.is_boolean() && defaultValueSettingType == SettingType::Boolean ||
        overrideValue.is_string() && defaultValueSettingType == SettingType::String ||
        overrideValue.is_number_integer() && defaultValueSettingType == SettingType::Int ||
        overrideValue.is_number_float() && defaultValueSettingType == SettingType::Double) {
        EXPECT_FALSE(details.isDefaultValue);
        EXPECT_EQ(expectedReturnValue, Value(details.value));
        EXPECT_EQ(nullopt, details.errorMessage);
        EXPECT_EQ(nullptr, details.errorException);
    } else {
        EXPECT_TRUE(details.isDefaultValue);
        EXPECT_EQ(expectedReturnValue, Value(details.value));
        if (overrideValue.is_boolean() || overrideValue.is_string() || overrideValue.is_number()) {
            EXPECT_THAT(logger->text, ::testing::HasSubstr("The type of a setting must match the type of the specified default value."));
        } else {
            EXPECT_THAT(logger->text, ::testing::HasSubstr("Setting type is invalid."));
        }
    }
}

class OverrideValueTypeMismatchShouldBeHandledCorrectly_SimplifiedConfigTestSuite : public OverrideTestBase<::testing::TestWithParam<tuple<string, Value, Value>>> {};
INSTANTIATE_TEST_SUITE_P(OverrideTest, OverrideValueTypeMismatchShouldBeHandledCorrectly_SimplifiedConfigTestSuite, ::testing::Values(
    make_tuple("true", false, true),
    make_tuple("true", "", ""),
    make_tuple("true", 0, 0),
    make_tuple("true", 0.0, 0.0),
    make_tuple("\"text\"", false, false),
    make_tuple("\"text\"", "", "text"),
    make_tuple("\"text\"", 0, 0),
    make_tuple("\"text\"", 0.0, 0.0),
    make_tuple("42", false, false),
    make_tuple("42", "", ""),
    make_tuple("42", 0, 42),
    make_tuple("42", 0.0, 0.0),
    make_tuple("42.0", false, false),
    make_tuple("42.0", "", ""),
    make_tuple("42.0", 0, 0),
    make_tuple("42.0", 0.0, 42.0),
    make_tuple("3.14", false, false),
    make_tuple("3.14", "", ""),
    make_tuple("3.14", 0, 0),
    make_tuple("3.14", 0.0, 3.14),
    make_tuple("null", false, false),
    make_tuple("[]", false, false),
    make_tuple("{}", false, false)
));
TEST_P(OverrideValueTypeMismatchShouldBeHandledCorrectly_SimplifiedConfigTestSuite, OverrideValueTypeMismatchShouldBeHandledCorrectly_SimplifiedConfig) {
    auto [overrideValueJson, defaultValue, expectedReturnValue] = GetParam();
    const string key = "flag";

    auto filePath = createTemporaryFile(string_format(R"({ "flags": { "%s": %s } })", key.c_str(), overrideValueJson.c_str()));

    std::shared_ptr<TestLogger> testLogger = make_shared<TestLogger>(LOG_LEVEL_WARNING);
    ConfigCatOptions options;
    options.pollingMode = PollingMode::manualPoll();
    options.logger = testLogger;
    options.flagOverrides = make_shared<FileFlagOverrides>(filePath, LocalOnly);
    auto client = ConfigCatClient::get(kTestSdkKey, &options);

    auto defaultValueSettingType = static_cast<SettingValue>(Value(defaultValue)).getSettingType();
    if (holds_alternative<bool>(defaultValue)) {
        checkTypeMismatch(*client, testLogger, key, overrideValueJson, get<bool>(defaultValue), defaultValueSettingType, expectedReturnValue);
    } else if (holds_alternative<string>(defaultValue)) {
        checkTypeMismatch(*client, testLogger, key, overrideValueJson, get<string>(defaultValue), defaultValueSettingType, expectedReturnValue);
    } else if (holds_alternative<int>(defaultValue)) {
        checkTypeMismatch(*client, testLogger, key, overrideValueJson, get<int>(defaultValue), defaultValueSettingType, expectedReturnValue);
    } else if (holds_alternative<double>(defaultValue)) {
        checkTypeMismatch(*client, testLogger, key, overrideValueJson, get<double>(defaultValue), defaultValueSettingType, expectedReturnValue);
    } else {
        throw std::runtime_error("Return value type is invalid.");
    }

    ConfigCatClient::closeAll();
}
