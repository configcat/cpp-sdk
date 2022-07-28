#include <gtest/gtest.h>
#include "mock.h"
#include "configcat/configcatclient.h"
#include "configcat/utils.h"


using namespace configcat;
using namespace std;

class ConfigCatClientTest : public ::testing::Test {
public:
    static constexpr char kTestSdkKey[] = "TestSdkKey";
    static constexpr char kTestJsonFormat[] = R"({ "f": { "fakeKey": { "v": %s, "p": [], "r": [] } } })";
    ConfigCatClient* client = nullptr;
    shared_ptr<MockHttpSessionAdapter> mockHttpSessionAdapter = make_shared<MockHttpSessionAdapter>();

    void SetUp() override {
        // Using of designated initializers requires at least '/std:c++20'
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

TEST_F(ConfigCatClientTest, GetString) {
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
