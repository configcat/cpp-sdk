#include <gtest/gtest.h>
#include "mock.h"
#include "utils.h"
#include "configservice.h"
#include "configcat/configcatoptions.h"
#include "configcat/configcatclient.h"


using namespace configcat;
using namespace std;

TEST(ConfigCacheTest, CacheKey) {
    EXPECT_EQ("147c5b4c2b2d7c77e1605b1a4309f0ea6684a0c6", ConfigService::generateCacheKey("test1"));
    EXPECT_EQ("c09513b1756de9e4bc48815ec7a142b2441ed4d5", ConfigService::generateCacheKey("test2"));
}

TEST(ConfigCacheTest, CachePayload) {
    double nowInSeconds = 1686756435.8449;
    std::string etag = "test-etag";
    ConfigEntry entry(Config::fromJson(kTestJsonString), etag, kTestJsonString, nowInSeconds);
    EXPECT_EQ("1686756435844\n" + etag + "\n" + kTestJsonString, entry.serialize());
}

TEST(ConfigCatTest, InvalidCacheContent) {
    static constexpr char kTestJsonFormat[] = R"({ "f": { "testKey": { "v": %s, "p": [], "r": [] } } })";
    HookCallbacks hookCallbacks;
    auto hooks = make_shared<Hooks>();
    hooks->addOnError([&](const string& error) { hookCallbacks.onError(error); });
    auto configJsonString = string_format(kTestJsonFormat, R"("test")");
    auto configCache = make_shared<SingleValueCache>(ConfigEntry(
        Config::fromJson(configJsonString),
        "test-etag",
        configJsonString,
        getUtcNowSecondsSinceEpoch()).serialize()
    );

    ConfigCatOptions options;
    options.pollingMode = PollingMode::manualPoll();
    options.configCache = configCache;
    options.hooks = hooks;
    auto client = ConfigCatClient::get("test", &options);

    EXPECT_EQ("test", client->getValue("testKey", "default"));
    EXPECT_EQ(0, hookCallbacks.errorCallCount);

    // Invalid fetch time in cache
    configCache->value = "text\n"s + "test-etag\n" + string_format(kTestJsonFormat, R"("test2")");
    EXPECT_EQ("test", client->getValue("testKey", "default"));
    EXPECT_TRUE(hookCallbacks.error.find("Error occurred while reading the cache. Invalid fetch time: text") != std::string::npos);

    // Number of values is fewer than expected
    configCache->value = std::to_string(getUtcNowSecondsSinceEpoch()) + "\n" + string_format(kTestJsonFormat, R"("test2")");
    EXPECT_EQ("test", client->getValue("testKey", "default"));
    EXPECT_TRUE(hookCallbacks.error.find("Error occurred while reading the cache. Number of values is fewer than expected.") != std::string::npos);

    // Invalid config JSON
    configCache->value = std::to_string(getUtcNowSecondsSinceEpoch()) + "\n" + "test-etag\n" + "wrong-json";
    EXPECT_EQ("test", client->getValue("testKey", "default"));
    EXPECT_TRUE(hookCallbacks.error.find("Error occurred while reading the cache. Invalid config JSON: wrong-json.") != std::string::npos);

    ConfigCatClient::close(client);
}
