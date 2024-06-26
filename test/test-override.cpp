#include <gtest/gtest.h>
#include "configcat/mapoverridedatasource.h"
#include "configcat/fileoverridedatasource.h"
#include "configcat/configcatclient.h"
#include "configcatlogger.h"
#include "configcat/consolelogger.h"
#include "mock.h"
#include "test.h"
#include <filesystem>
#include <fstream>


using namespace configcat;
using namespace std;


class OverrideTest : public ::testing::Test {
public:
    ~OverrideTest() {
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
