#include <gtest/gtest.h>
#include "configcat/configcatclient.h"

using namespace configcat;
using namespace std;

class ConfigCatClientTest : public ::testing::Test {
public:
    static constexpr char testSdkKey[] = "testSdkKey";
    ConfigCatClient* client = nullptr;

    void SetUp() override {
        client = ConfigCatClient::get(testSdkKey, ConfigCatOptions());
    }

    void TearDown() override {
        ConfigCatClient::close();
    }
};

TEST_F(ConfigCatClientTest, EnsureSingletonPerSdkKey) {
    auto client2 = ConfigCatClient::get(testSdkKey);
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
