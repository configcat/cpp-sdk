#include <gtest/gtest.h>
#include "mock.h"
#include "utils.h"
#include "configservice.h"
#include "configcat/configcatoptions.h"
#include "configcat/configcatlogger.h"
#include "configcat/consolelogger.h"
#include <thread>
#include <chrono>

using namespace configcat;
using namespace std;
using namespace std::chrono;
using namespace std::this_thread;

class AutoPollingTest : public ::testing::Test {
public:
    static constexpr char kTestSdkKey[] = "TestSdkKey";
    static constexpr char kTestJsonFormat[] = R"({"f":{"fakeKey":{"t":%d,"v":%s}}})";

    shared_ptr<MockHttpSessionAdapter> mockHttpSessionAdapter = make_shared<MockHttpSessionAdapter>();
    shared_ptr<ConfigCatLogger> logger = make_shared<ConfigCatLogger>(make_shared<ConsoleLogger>(), make_shared<Hooks>());
};

TEST_F(AutoPollingTest, Get) {
    configcat::Response firstResponse = {200, string_format(kTestJsonFormat, SettingType::String, R"({"s":"test"})")};
    mockHttpSessionAdapter->enqueueResponse(firstResponse);
    configcat::Response secondResponse = {200, string_format(kTestJsonFormat, SettingType::String, R"({"s":"test2"})")};
    mockHttpSessionAdapter->enqueueResponse(secondResponse);

    ConfigCatOptions options;
    options.pollingMode = PollingMode::autoPoll(2);
    options.httpSessionAdapter = mockHttpSessionAdapter;
    auto service = ConfigService(kTestSdkKey, logger, make_shared<Hooks>(), make_shared<NullConfigCache>(), options);

    auto settings = *service.getSettings().settings;
    EXPECT_EQ("test", std::get<string>(settings["fakeKey"].value));

    sleep_for(seconds(3));

    settings = *service.getSettings().settings;
    EXPECT_EQ("test2", std::get<string>(settings["fakeKey"].value));
}

TEST_F(AutoPollingTest, GetFailedRequest) {
    configcat::Response firstResponse = {200, string_format(kTestJsonFormat, SettingType::String, R"({"s":"test"})")};
    mockHttpSessionAdapter->enqueueResponse(firstResponse);
    configcat::Response secondResponse = {500, string_format(kTestJsonFormat, SettingType::String, R"({"s":"test2"})")};
    mockHttpSessionAdapter->enqueueResponse(secondResponse);

    ConfigCatOptions options;
    options.pollingMode = PollingMode::autoPoll(2);
    options.httpSessionAdapter = mockHttpSessionAdapter;
    auto service = ConfigService(kTestSdkKey, logger, make_shared<Hooks>(), make_shared<NullConfigCache>(), options);

    auto settings = *service.getSettings().settings;
    EXPECT_EQ("test", std::get<string>(settings["fakeKey"].value));

    sleep_for(seconds(3));

    settings = *service.getSettings().settings;
    EXPECT_EQ("test", std::get<string>(settings["fakeKey"].value));
}

TEST_F(AutoPollingTest, OnConfigChanged) {
    configcat::Response firstResponse = {200, string_format(kTestJsonFormat, SettingType::String, R"({"s":"test"})")};
    mockHttpSessionAdapter->enqueueResponse(firstResponse);
    configcat::Response secondResponse = {200, string_format(kTestJsonFormat, SettingType::String, R"({"s":"test2"})")};
    mockHttpSessionAdapter->enqueueResponse(secondResponse);

    bool called = false;
    auto hooks = make_shared<Hooks>();
    hooks->addOnConfigChanged([&](auto config){ called = true; });
    ConfigCatOptions options;
    options.pollingMode = PollingMode::autoPoll(2, 5);
    options.httpSessionAdapter = mockHttpSessionAdapter;
    auto service = ConfigService(kTestSdkKey, logger, hooks, make_shared<NullConfigCache>(), options);

    sleep_for(seconds(1));

    EXPECT_TRUE(called);

    sleep_for(seconds(3));

    auto settings = *service.getSettings().settings;
    EXPECT_EQ("test2", std::get<string>(settings["fakeKey"].value));
}

TEST_F(AutoPollingTest, RequestTimeout) {
    configcat::Response response = {200, string_format(kTestJsonFormat, SettingType::String, R"({"s":"test"})")};
    constexpr int responseDelay = 3;
    mockHttpSessionAdapter->enqueueResponse(response, responseDelay);

    ConfigCatOptions options;
    options.pollingMode = PollingMode::autoPoll(1);
    options.httpSessionAdapter = mockHttpSessionAdapter;
    auto service = ConfigService(kTestSdkKey, logger, make_shared<Hooks>(), make_shared<NullConfigCache>(), options);

    sleep_for(seconds(2));

    EXPECT_EQ(1, mockHttpSessionAdapter->requests.size());

    sleep_for(milliseconds(3500));

    auto settings = service.getSettings().settings;
    ASSERT_TRUE(settings != nullptr);
    EXPECT_EQ("test", std::get<string>(settings->at("fakeKey").value));
}

TEST_F(AutoPollingTest, InitWaitTimeout) {
    configcat::Response response = {200, string_format(kTestJsonFormat, SettingType::String, R"({"s":"test"})")};
    constexpr int responseDelay = 5;
    mockHttpSessionAdapter->enqueueResponse(response, responseDelay);

    auto startTime = std::chrono::steady_clock::now();
    ConfigCatOptions options;
    options.pollingMode = PollingMode::autoPoll(60, 1);
    options.httpSessionAdapter = mockHttpSessionAdapter;
    auto service = ConfigService(kTestSdkKey, logger, make_shared<Hooks>(), make_shared<NullConfigCache>(), options);

    auto settings = service.getSettings().settings;
    EXPECT_EQ(settings, nullptr);

    auto endTime = std::chrono::steady_clock::now();
    auto elapsedTimeInSeconds = std::chrono::duration<double>(endTime - startTime).count();

    EXPECT_TRUE(elapsedTimeInSeconds > 1);
    EXPECT_TRUE(elapsedTimeInSeconds < 2);
}

TEST_F(AutoPollingTest, CancelRequest) {
    configcat::Response response = {200, string_format(kTestJsonFormat, SettingType::String, R"({"s":"test"})")};
    constexpr int responseDelay = 60;
    mockHttpSessionAdapter->enqueueResponse(response, responseDelay);

    auto startTime = std::chrono::steady_clock::now();
    ConfigCatOptions options;
    options.pollingMode = PollingMode::autoPoll(2);
    options.httpSessionAdapter = mockHttpSessionAdapter;
    auto service = ConfigService(kTestSdkKey, logger, make_shared<Hooks>(), make_shared<NullConfigCache>(), options);

    auto settings = service.getSettings().settings;
    EXPECT_EQ(settings, nullptr);

    EXPECT_EQ(1, mockHttpSessionAdapter->responses.size());
}

TEST_F(AutoPollingTest, Cache) {
    auto mockCache = make_shared<InMemoryConfigCache>();

    configcat::Response firstResponse = {200, string_format(kTestJsonFormat, SettingType::String, R"({"s":"test"})")};
    mockHttpSessionAdapter->enqueueResponse(firstResponse);
    configcat::Response secondResponse = {200, string_format(kTestJsonFormat, SettingType::String, R"({"s":"test2"})")};
    mockHttpSessionAdapter->enqueueResponse(secondResponse);

    ConfigCatOptions options;
    options.pollingMode = PollingMode::autoPoll(2);
    options.httpSessionAdapter = mockHttpSessionAdapter;
    auto service = ConfigService(kTestSdkKey, logger, make_shared<Hooks>(), mockCache, options);

    auto settings = *service.getSettings().settings;
    EXPECT_EQ("test", std::get<string>(settings["fakeKey"].value));

    EXPECT_EQ(1, mockCache->store.size());
    EXPECT_TRUE(contains(mockCache->store.begin()->second, R"({"s":"test"})"));

    sleep_for(seconds(3));

    settings = *service.getSettings().settings;
    EXPECT_EQ("test2", std::get<string>(settings["fakeKey"].value));

    EXPECT_EQ(1, mockCache->store.size());
    EXPECT_TRUE(contains(mockCache->store.begin()->second, R"({"s":"test2"})"));
}

TEST_F(AutoPollingTest, ReturnCachedConfigWhenCacheIsNotExpired) {
    auto jsonString = string_format(kTestJsonFormat, SettingType::String, R"({"s":"test"})");
    auto mockCache = make_shared<SingleValueCache>(ConfigEntry(
        Config::fromJson(jsonString),
        "test-etag",
        jsonString,
        getUtcNowSecondsSinceEpoch()).serialize()
    );

    configcat::Response firstResponse = {200, string_format(kTestJsonFormat, SettingType::String, R"({"s":"test2"})")};
    mockHttpSessionAdapter->enqueueResponse(firstResponse);

    auto pollIntervalSeconds = 2;
    auto maxInitWaitTimeSeconds = 1;
    ConfigCatOptions options;
    options.pollingMode = PollingMode::autoPoll(pollIntervalSeconds, maxInitWaitTimeSeconds);
    options.httpSessionAdapter = mockHttpSessionAdapter;

    auto startTime = chrono::steady_clock::now();

    auto service = ConfigService(kTestSdkKey, logger, make_shared<Hooks>(), mockCache, options);
    auto settings = *service.getSettings().settings;

    auto endTime = chrono::steady_clock::now();
    auto elapsedTimeInSeconds = chrono::duration<double>(endTime - startTime).count();

    // max init wait time should be ignored when cache is not expired
    EXPECT_LE(elapsedTimeInSeconds, maxInitWaitTimeSeconds);

    EXPECT_EQ("test", std::get<string>(settings["fakeKey"].value));
    EXPECT_EQ(0, mockHttpSessionAdapter->requests.size());

    sleep_for(seconds(3));

    settings = *service.getSettings().settings;

    EXPECT_EQ("test2", std::get<string>(settings["fakeKey"].value));
    EXPECT_EQ(1, mockHttpSessionAdapter->requests.size());
}

TEST_F(AutoPollingTest, FetchConfigWhenCacheIsExpired) {
    auto pollIntervalSeconds = 2;
    auto maxInitWaitTimeSeconds = 1;
    auto jsonString = string_format(kTestJsonFormat, SettingType::String, R"({"s":"test"})");
    auto mockCache = make_shared<SingleValueCache>(ConfigEntry(
        Config::fromJson(jsonString),
        "test-etag",
        jsonString,
        getUtcNowSecondsSinceEpoch() - pollIntervalSeconds).serialize()
    );

    configcat::Response firstResponse = {200, string_format(kTestJsonFormat, SettingType::String, R"({"s":"test2"})")};
    mockHttpSessionAdapter->enqueueResponse(firstResponse);

    ConfigCatOptions options;
    options.pollingMode = PollingMode::autoPoll(pollIntervalSeconds, maxInitWaitTimeSeconds);
    options.httpSessionAdapter = mockHttpSessionAdapter;
    auto service = ConfigService(kTestSdkKey, logger, make_shared<Hooks>(), mockCache, options);

    auto settings = *service.getSettings().settings;
    EXPECT_EQ("test2", std::get<string>(settings["fakeKey"].value));
    EXPECT_EQ(1, mockHttpSessionAdapter->requests.size());
}

TEST_F(AutoPollingTest, initWaitTimeReturnCached) {
    auto pollIntervalSeconds = 60;
    auto maxInitWaitTimeSeconds = 1;
    auto jsonString = string_format(kTestJsonFormat, SettingType::String, R"({"s":"test"})");
    auto mockCache = make_shared<SingleValueCache>(ConfigEntry(
        Config::fromJson(jsonString),
        "test-etag",
        jsonString,
        getUtcNowSecondsSinceEpoch() - 2 * pollIntervalSeconds).serialize()
    );

    configcat::Response response = {200, string_format(kTestJsonFormat, SettingType::String, R"({"s":"test2"})")};
    constexpr int responseDelay = 5;
    mockHttpSessionAdapter->enqueueResponse(response, responseDelay);


    ConfigCatOptions options;
    options.pollingMode = PollingMode::autoPoll(pollIntervalSeconds, maxInitWaitTimeSeconds);
    options.httpSessionAdapter = mockHttpSessionAdapter;

    auto startTime = chrono::steady_clock::now();

    auto service = ConfigService(kTestSdkKey, logger, make_shared<Hooks>(), mockCache, options);
    auto settings = *service.getSettings().settings;

    auto endTime = chrono::steady_clock::now();
    auto elapsedTimeInSeconds = chrono::duration<double>(endTime - startTime).count();

    EXPECT_GT(elapsedTimeInSeconds, maxInitWaitTimeSeconds);
    EXPECT_LE(elapsedTimeInSeconds, maxInitWaitTimeSeconds + 1);
    EXPECT_EQ("test", std::get<string>(settings["fakeKey"].value));
}

TEST_F(AutoPollingTest, OnlineOffline) {
    configcat::Response response = {200, string_format(kTestJsonFormat, SettingType::String, R"({"s":"test"})")};
    mockHttpSessionAdapter->enqueueResponse(response);

    ConfigCatOptions options;
    options.pollingMode = PollingMode::autoPoll(1);
    options.httpSessionAdapter = mockHttpSessionAdapter;
    auto service = ConfigService(kTestSdkKey, logger, make_shared<Hooks>(), make_shared<NullConfigCache>(), options);

    EXPECT_FALSE(service.isOffline());

    sleep_for(milliseconds(1500));

    service.setOffline();
    EXPECT_TRUE(service.isOffline());
    auto settings = *service.getSettings().settings;
    EXPECT_EQ("test", std::get<string>(settings["fakeKey"].value));
    EXPECT_EQ(2, mockHttpSessionAdapter->requests.size());

    sleep_for(seconds(2));

    EXPECT_EQ(2, mockHttpSessionAdapter->requests.size());
    service.setOnline();
    EXPECT_FALSE(service.isOffline());

    sleep_for(seconds(1));

    EXPECT_TRUE(mockHttpSessionAdapter->requests.size() >= 3);
}

TEST_F(AutoPollingTest, InitOffline) {
    configcat::Response response = {200, string_format(kTestJsonFormat, SettingType::String, R"({"s":"test"})")};
    mockHttpSessionAdapter->enqueueResponse(response);

    ConfigCatOptions options;
    options.pollingMode = PollingMode::autoPoll(1);
    options.httpSessionAdapter = mockHttpSessionAdapter;
    options.offline = true;
    auto service = ConfigService(kTestSdkKey, logger, make_shared<Hooks>(), make_shared<NullConfigCache>(), options);

    EXPECT_TRUE(service.isOffline());
    auto settings = service.getSettings().settings;
    EXPECT_TRUE(settings == nullptr);
    EXPECT_EQ(0, mockHttpSessionAdapter->requests.size());

    sleep_for(seconds(2));

    settings = service.getSettings().settings;
    EXPECT_TRUE(settings == nullptr);
    EXPECT_EQ(0, mockHttpSessionAdapter->requests.size());

    service.setOnline();
    EXPECT_FALSE(service.isOffline());

    sleep_for(milliseconds(2500));

    settings = service.getSettings().settings;
    EXPECT_EQ("test", std::get<string>((*settings)["fakeKey"].value));
    EXPECT_TRUE(mockHttpSessionAdapter->requests.size() >= 2);
}
