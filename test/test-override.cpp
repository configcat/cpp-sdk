#include <gtest/gtest.h>
#include "configcat/mapoverridedatasource.h"
#include "configcat/configcatclient.h"
#include "mock.h"
#include "utils.h"

using namespace configcat;
using namespace std;


class OverrideTest : public ::testing::Test {
public:
    static constexpr char kTestSdkKey[] = "TestSdkKey";
    static constexpr char kTestJsonFormat[] = R"({ "f": { "fakeKey": { "v": %s, "p": [], "r": [] } } })";
    ConfigCatClient* client = nullptr;
    shared_ptr<MockHttpSessionAdapter> mockHttpSessionAdapter = make_shared<MockHttpSessionAdapter>();
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
    options.mode = PollingMode::manualPoll();
    options.override = make_shared<FlagOverrides>(make_shared<MapOverrideDataSource>(map), LocalOnly);
    auto client = ConfigCatClient::get(kTestSdkKey, options);

    EXPECT_TRUE(client->getValue("enabledFeature", false));
    EXPECT_FALSE(client->getValue("disabledFeature", true));
    EXPECT_EQ(5, client->getValue("intSetting", 0));
    EXPECT_EQ(3.14, client->getValue("doubleSetting", 0.0));
    EXPECT_EQ("test", client->getValue("stringSetting", ""));

    ConfigCatClient::close();
}

TEST_F(OverrideTest, LocalOverRemote) {
    configcat::Response response = {200, string_format(kTestJsonFormat, "false")};
    mockHttpSessionAdapter->enqueueResponse(response);

    const std::unordered_map<std::string, Value>& map = {
        { "fakeKey", true },
        { "nonexisting", true }
    };

    ConfigCatOptions options;
    options.mode = PollingMode::manualPoll();
    options.httpSessionAdapter = mockHttpSessionAdapter;
    options.override = make_shared<FlagOverrides>(make_shared<MapOverrideDataSource>(map), LocalOverRemote);
    auto client = ConfigCatClient::get(kTestSdkKey, options);
    client->forceRefresh();

    EXPECT_TRUE(client->getValue("fakeKey", false));
    EXPECT_TRUE(client->getValue("nonexisting", false));

    ConfigCatClient::close();
}

TEST_F(OverrideTest, RemoteOverLocal) {
    configcat::Response response = {200, string_format(kTestJsonFormat, "false")};
    mockHttpSessionAdapter->enqueueResponse(response);

    const std::unordered_map<std::string, Value>& map = {
        { "fakeKey", true },
        { "nonexisting", true }
    };

    ConfigCatOptions options;
    options.mode = PollingMode::manualPoll();
    options.httpSessionAdapter = mockHttpSessionAdapter;
    options.override = make_shared<FlagOverrides>(make_shared<MapOverrideDataSource>(map), RemoteOverLocal);
    auto client = ConfigCatClient::get(kTestSdkKey, options);
    client->forceRefresh();

    EXPECT_FALSE(client->getValue("fakeKey", true));
    EXPECT_TRUE(client->getValue("nonexisting", false));

    ConfigCatClient::close();
}
