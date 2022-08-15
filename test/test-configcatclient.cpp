#include <gtest/gtest.h>
#include "mock.h"
#include "configcat/configcatclient.h"
#include "configcat/config.h"
#include "utils.h"


using namespace configcat;
using namespace std;

class ConfigCatClientTest : public ::testing::Test {
public:
    static constexpr char kTestSdkKey[] = "TestSdkKey";
    static constexpr char kTestJsonFormat[] = R"({ "f": { "fakeKey": { "v": %s, "p": [], "r": [] } } })";
    static constexpr char kTestJsonMultiple[] = R"({ "f": { "key1": { "v": true, "i": "fakeId1", "p": [], "r": [] }, "key2": { "v": false, "i": "fakeId2", "p": [], "r": [] } } })";

    ConfigCatClient* client = nullptr;
    shared_ptr<MockHttpSessionAdapter> mockHttpSessionAdapter = make_shared<MockHttpSessionAdapter>();

    void SetUp() override {
        ConfigCatOptions options;
        options.mode = PollingMode::manualPoll();
        options.httpSessionAdapter = mockHttpSessionAdapter;

        client = ConfigCatClient::get(kTestSdkKey, options);
    }

    void TearDown() override {
        ConfigCatClient::close();
    }
};

TEST_F(ConfigCatClientTest, EnsureSingletonPerSdkKey) {
    auto client2 = ConfigCatClient::get(kTestSdkKey);
    EXPECT_TRUE(client2 == client);
}

TEST_F(ConfigCatClientTest, EnsureCloseWorks) {
    ConfigCatClient::close();
    EXPECT_TRUE(ConfigCatClient::instanceCount() == 0);

    auto client = ConfigCatClient::get("another");
    auto client2 = ConfigCatClient::get("another");
    EXPECT_TRUE(client2 == client);
    EXPECT_TRUE(ConfigCatClient::instanceCount() == 1);

    ConfigCatClient::close(client2);
    EXPECT_TRUE(ConfigCatClient::instanceCount() == 0);

    client = ConfigCatClient::get("another");
    EXPECT_TRUE(ConfigCatClient::instanceCount() == 1);

    ConfigCatClient::close();
    EXPECT_TRUE(ConfigCatClient::instanceCount() == 0);

    client = ConfigCatClient::get("another");
    EXPECT_TRUE(ConfigCatClient::instanceCount() == 1);
}

TEST_F(ConfigCatClientTest, GetIntValue) {
    configcat::Response response = {200, string_format(kTestJsonFormat, "43")};
    mockHttpSessionAdapter->enqueueResponse(response);
    client->forceRefresh();
    auto value = client->getValue("fakeKey", 10);

    EXPECT_EQ(43, value);
}

TEST_F(ConfigCatClientTest, GetIntValueFailed) {
    configcat::Response response = {200, string_format(kTestJsonFormat, R"("fake")")};
    mockHttpSessionAdapter->enqueueResponse(response);
    client->forceRefresh();
    auto value = client->getValue("fakeKey", 10);

    EXPECT_EQ(10, value);
}

TEST_F(ConfigCatClientTest, GetIntValueFailedInvalidJson) {
    configcat::Response response = {200, "{"};
    mockHttpSessionAdapter->enqueueResponse(response);
    client->forceRefresh();
    auto value = client->getValue("fakeKey", 10);

    EXPECT_EQ(10, value);
}

TEST_F(ConfigCatClientTest, GetIntValueFailedPartialJson) {
    configcat::Response responseWithoutValue = {200, R"({ "f": { "fakeKey": { "p": [], "r": [] } } })"};
    mockHttpSessionAdapter->enqueueResponse(responseWithoutValue);
    client->forceRefresh();
    auto value = client->getValue("fakeKey", 10);

    EXPECT_EQ(10, value);
}

TEST_F(ConfigCatClientTest, GetIntValueFailedNullValueJson) {
    configcat::Response responseWithoutValue = {200, R"({ "f": { "fakeKey": { "v": null, "p": [], "r": [] } } })"};
    mockHttpSessionAdapter->enqueueResponse(responseWithoutValue);
    client->forceRefresh();
    auto value = client->getValue("fakeKey", 10);

    EXPECT_EQ(10, value);
}

TEST_F(ConfigCatClientTest, GetStringValue) {
    configcat::Response response = {200, string_format(kTestJsonFormat, R"("fake")")};
    mockHttpSessionAdapter->enqueueResponse(response);
    client->forceRefresh();
    auto value = client->getValue("fakeKey", "default");

    EXPECT_EQ("fake", value);
}

TEST_F(ConfigCatClientTest, GetStringValueFailed) {
    configcat::Response response = {200, string_format(kTestJsonFormat, "33")};
    mockHttpSessionAdapter->enqueueResponse(response);
    client->forceRefresh();
    auto value = client->getValue("fakeKey", "default");

    EXPECT_EQ("default", value);
}

TEST_F(ConfigCatClientTest, GetDoubleValue) {
    configcat::Response response = {200, string_format(kTestJsonFormat, "43.56")};
    mockHttpSessionAdapter->enqueueResponse(response);
    client->forceRefresh();
    auto value = client->getValue("fakeKey", 3.14);

    EXPECT_EQ(43.56, value);
}

TEST_F(ConfigCatClientTest, GetDoubleValueFailed) {
    configcat::Response response = {200, string_format(kTestJsonFormat, R"("fake")")};
    mockHttpSessionAdapter->enqueueResponse(response);
    client->forceRefresh();
    auto value = client->getValue("fakeKey", 3.14);

    EXPECT_EQ(3.14, value);
}

TEST_F(ConfigCatClientTest, GetBoolValue) {
    configcat::Response response = {200, string_format(kTestJsonFormat, "true")};
    mockHttpSessionAdapter->enqueueResponse(response);
    client->forceRefresh();
    auto value = client->getValue("fakeKey", false);

    EXPECT_TRUE(value);
}

TEST_F(ConfigCatClientTest, GetBoolValueFailed) {
    configcat::Response response = {200, string_format(kTestJsonFormat, R"("fake")")};
    mockHttpSessionAdapter->enqueueResponse(response);
    client->forceRefresh();
    auto value = client->getValue("fakeKey", false);

    EXPECT_FALSE(value);
}

TEST_F(ConfigCatClientTest, GetAllKeys) {
    configcat::Response response = {200, kTestJsonMultiple};
    mockHttpSessionAdapter->enqueueResponse(response);
    client->forceRefresh();
    auto keys = client->getAllKeys();

    EXPECT_EQ(2, keys.size());
    EXPECT_TRUE(std::find(keys.begin(), keys.end(), "key1") != keys.end());
    EXPECT_TRUE(std::find(keys.begin(), keys.end(), "key2") != keys.end());
}

TEST_F(ConfigCatClientTest, GetAllValues) {
    configcat::Response response = {200, kTestJsonMultiple};
    mockHttpSessionAdapter->enqueueResponse(response);
    client->forceRefresh();
    auto allValues = client->getAllValues();

    EXPECT_EQ(2, allValues.size());
    EXPECT_EQ(true, std::get<bool>(allValues.at("key1")));
    EXPECT_EQ(false, std::get<bool>(allValues.at("key2")));
}








TEST_F(ConfigCatClientTest, GetValueTest) {
    auto boolValue = client->getValue(string("bool"), false);
    auto boolValue2 = client->getValue("bool", false);
    const char* def = "default";
    auto stringValue = client->getValue(string("string"), def);
    char def2[100] = "def";
    auto stringValue2 = client->getValue(string("string"), def2);
    char* def3 = def2;
    auto stringValue3 = client->getValue(string("string"), def3);
    auto strValue = client->getValue(string("string"), "default");
    auto strValue2 = client->getValue("string", "default");
    auto intValue = client->getValue(string("int"), 0);
    auto otherValue = client->getValue(string("other"), 0.0);
}
