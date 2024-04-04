#include <gtest/gtest.h>
#include "mock.h"
#include "configservice.h"
#include "configcat/configcatoptions.h"
#include "configcat/configcatclient.h"
#include "configcat/timeutils.h"


using namespace configcat;
using namespace std;

TEST(ConfigCacheTest, CacheKey) {
    EXPECT_EQ("f83ba5d45bceb4bb704410f51b704fb6dfa19942", ConfigService::generateCacheKey("configcat-sdk-1/TEST_KEY-0123456789012/1234567890123456789012"));
    EXPECT_EQ("da7bfd8662209c8ed3f9db96daed4f8d91ba5876", ConfigService::generateCacheKey("configcat-sdk-1/TEST_KEY2-123456789012/1234567890123456789012"));
}

TEST(ConfigCacheTest, CachePayload) {
    double nowInSeconds = 1686756435.8449;
    std::string etag = "test-etag";
    ConfigEntry entry(Config::fromJson(kTestJsonString), etag, kTestJsonString, nowInSeconds);
    EXPECT_EQ("1686756435844\n" + etag + "\n" + kTestJsonString, entry.serialize());
}

TEST(ConfigCatTest, InvalidCacheContent) {
    static constexpr char kTestJsonFormat[] = R"({"f":{"testKey":{"t":%d,"v":%s}}})";
    HookCallbacks hookCallbacks;
    auto hooks = make_shared<Hooks>();
    hooks->addOnError([&](const string& message, const exception_ptr& exception) { hookCallbacks.onError(message, exception); });
    auto configJsonString = string_format(kTestJsonFormat, SettingType::String, R"({"s":"test"})");
    auto configCache = make_shared<SingleValueCache>(ConfigEntry(
        Config::fromJson(configJsonString),
        "test-etag",
        configJsonString,
        get_utcnowseconds_since_epoch()).serialize()
    );

    ConfigCatOptions options;
    options.pollingMode = PollingMode::manualPoll();
    options.configCache = configCache;
    options.hooks = hooks;
    auto client = ConfigCatClient::get("test-67890123456789012/1234567890123456789012", &options);

    EXPECT_EQ("test", client->getValue("testKey", "default"));
    EXPECT_EQ(0, hookCallbacks.errorCallCount);

    // Invalid fetch time in cache
    configCache->value = "text\n"s + "test-etag\n" + string_format(kTestJsonFormat, SettingType::String, R"({"s":"test2"})");
    EXPECT_EQ("test", client->getValue("testKey", "default"));
    EXPECT_EQ("Error occurred while reading the cache.", hookCallbacks.errorMessage);
    EXPECT_TRUE(unwrap_exception_message(hookCallbacks.errorException).find("Invalid fetch time: text") != string::npos);

    // Number of values is fewer than expected
    configCache->value = std::to_string(get_utcnowseconds_since_epoch()) + "\n" + string_format(kTestJsonFormat, SettingType::String, R"({"s":"test2"})");
    EXPECT_EQ("test", client->getValue("testKey", "default"));
    EXPECT_TRUE(hookCallbacks.errorMessage.find("Error occurred while reading the cache.") != std::string::npos);
    EXPECT_TRUE(unwrap_exception_message(hookCallbacks.errorException).find("Number of values is fewer than expected.") != string::npos);

    // Invalid config JSON
    configCache->value = std::to_string(get_utcnowseconds_since_epoch()) + "\n" + "test-etag\n" + "wrong-json";
    EXPECT_EQ("test", client->getValue("testKey", "default"));
    EXPECT_TRUE(hookCallbacks.errorMessage.find("Error occurred while reading the cache.") != std::string::npos);
    EXPECT_TRUE(unwrap_exception_message(hookCallbacks.errorException).find("Invalid config JSON: wrong-json.") != string::npos);

    ConfigCatClient::close(client);
}
