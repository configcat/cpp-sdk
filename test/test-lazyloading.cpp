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

class LazyLoadingTest : public ::testing::Test {
public:
    static constexpr char kTestSdkKey[] = "TestSdkKey";
    static constexpr char kTestJsonFormat[] = R"({ "f": { "fakeKey": { "v": %s, "p": [], "r": [] } } })";

    shared_ptr<MockHttpSessionAdapter> mockHttpSessionAdapter = make_shared<MockHttpSessionAdapter>();
    shared_ptr<ConfigCatLogger> logger = make_shared<ConfigCatLogger>(make_shared<ConsoleLogger>(), make_shared<Hooks>());
};

TEST_F(LazyLoadingTest, Get) {
    configcat::Response firstResponse = {200, string_format(kTestJsonFormat, R"("test")")};
    mockHttpSessionAdapter->enqueueResponse(firstResponse);
    configcat::Response secondResponse = {200, string_format(kTestJsonFormat, R"("test2")")};
    constexpr int secondResponseDelay = 2;
    mockHttpSessionAdapter->enqueueResponse(secondResponse, secondResponseDelay);

    ConfigCatOptions options;
    options.pollingMode = PollingMode::lazyLoad(2);
    options.httpSessionAdapter = mockHttpSessionAdapter;
    auto service = ConfigService(kTestSdkKey, logger, make_shared<Hooks>(), make_shared<NullConfigCache>(), options);

    auto settings = *service.getSettings().settings;
    EXPECT_EQ("test", std::get<string>(settings["fakeKey"].value));

    settings = *service.getSettings().settings;
    EXPECT_EQ("test", std::get<string>(settings["fakeKey"].value));

    EXPECT_EQ(1, mockHttpSessionAdapter->requests.size());

    // Wait for cache invalidation
    sleep_for(seconds(3));

    settings = *service.getSettings().settings;
    EXPECT_EQ("test2", std::get<string>(settings["fakeKey"].value));
}

TEST_F(LazyLoadingTest, GetFailedRequest) {
    configcat::Response firstResponse = {200, string_format(kTestJsonFormat, R"("test")")};
    mockHttpSessionAdapter->enqueueResponse(firstResponse);
    configcat::Response secondResponse = {500, string_format(kTestJsonFormat, R"("test2")")};
    mockHttpSessionAdapter->enqueueResponse(secondResponse);

    ConfigCatOptions options;
    options.pollingMode = PollingMode::lazyLoad(2);
    options.httpSessionAdapter = mockHttpSessionAdapter;
    auto service = ConfigService(kTestSdkKey, logger, make_shared<Hooks>(), make_shared<NullConfigCache>(), options);

    auto settings = *service.getSettings().settings;
    EXPECT_EQ("test", std::get<string>(settings["fakeKey"].value));

    settings = *service.getSettings().settings;
    EXPECT_EQ("test", std::get<string>(settings["fakeKey"].value));

    EXPECT_EQ(1, mockHttpSessionAdapter->requests.size());

    // Wait for cache invalidation
    sleep_for(seconds(3));

    settings = *service.getSettings().settings;
    EXPECT_EQ("test", std::get<string>(settings["fakeKey"].value));
}

TEST_F(LazyLoadingTest, Cache) {
    auto mockCache = make_shared<InMemoryConfigCache>();

    configcat::Response firstResponse = {200, string_format(kTestJsonFormat, R"("test")")};
    mockHttpSessionAdapter->enqueueResponse(firstResponse);
    configcat::Response secondResponse = {200, string_format(kTestJsonFormat, R"("test2")")};
    mockHttpSessionAdapter->enqueueResponse(secondResponse);

    ConfigCatOptions options;
    options.pollingMode = PollingMode::lazyLoad(2);
    options.httpSessionAdapter = mockHttpSessionAdapter;
    auto service = ConfigService(kTestSdkKey, logger, make_shared<Hooks>(), mockCache, options);

    auto settings = *service.getSettings().settings;
    EXPECT_EQ("test", std::get<string>(settings["fakeKey"].value));

    EXPECT_EQ(1, mockCache->store.size());
    EXPECT_TRUE(contains(mockCache->store.begin()->second, R"("test")"));

    // Wait for cache invalidation
    sleep_for(seconds(3));

    settings = *service.getSettings().settings;
    EXPECT_EQ("test2", std::get<string>(settings["fakeKey"].value));

    EXPECT_EQ(1, mockCache->store.size());
    EXPECT_TRUE(contains(mockCache->store.begin()->second, R"("test2")"));
}

TEST_F(LazyLoadingTest, ReturnCachedConfigWhenCacheIsNotExpired) {
    auto mockCache = make_shared<SingleValueCache>(R"({"config":)"s
        + string_format(kTestJsonFormat, R"("test")") + R"(,"etag":"test-etag")"
        + R"(,"fetch_time":)" + to_string(getUtcNowSecondsSinceEpoch()) + "}");

    configcat::Response firstResponse = {200, string_format(kTestJsonFormat, R"("test2")")};
    mockHttpSessionAdapter->enqueueResponse(firstResponse);

    ConfigCatOptions options;
    options.pollingMode = PollingMode::lazyLoad(1);
    options.httpSessionAdapter = mockHttpSessionAdapter;
    auto service = ConfigService(kTestSdkKey, logger, make_shared<Hooks>(), mockCache, options);

    auto settings = *service.getSettings().settings;

    EXPECT_EQ("test", std::get<string>(settings["fakeKey"].value));
    EXPECT_EQ(0, mockHttpSessionAdapter->requests.size());

    sleep_for(seconds(1));

    settings = *service.getSettings().settings;

    EXPECT_EQ("test2", std::get<string>(settings["fakeKey"].value));
    EXPECT_EQ(1, mockHttpSessionAdapter->requests.size());
}

TEST_F(LazyLoadingTest, FetchConfigWhenCacheIsExpired) {
    auto cacheTimeToLiveSeconds = 1;
    auto mockCache = make_shared<SingleValueCache>(R"({"config":)"s
        + string_format(kTestJsonFormat, R"("test")") + R"(,"etag":"test-etag")"
        + R"(,"fetch_time":)" + to_string(getUtcNowSecondsSinceEpoch() - cacheTimeToLiveSeconds) + "}");

    configcat::Response firstResponse = {200, string_format(kTestJsonFormat, R"("test2")")};
    mockHttpSessionAdapter->enqueueResponse(firstResponse);

    ConfigCatOptions options;
    options.pollingMode = PollingMode::lazyLoad(cacheTimeToLiveSeconds);
    options.httpSessionAdapter = mockHttpSessionAdapter;
    auto service = ConfigService(kTestSdkKey, logger, make_shared<Hooks>(), mockCache, options);

    auto settings = *service.getSettings().settings;
    EXPECT_EQ("test2", std::get<string>(settings["fakeKey"].value));
    EXPECT_EQ(1, mockHttpSessionAdapter->requests.size());
}

TEST_F(LazyLoadingTest, OnlineOffline) {
    configcat::Response response = {200, string_format(kTestJsonFormat, R"("test")")};
    mockHttpSessionAdapter->enqueueResponse(response);

    ConfigCatOptions options;
    options.pollingMode = PollingMode::lazyLoad(1);
    options.httpSessionAdapter = mockHttpSessionAdapter;
    auto service = ConfigService(kTestSdkKey, logger, make_shared<Hooks>(), make_shared<NullConfigCache>(), options);

    EXPECT_FALSE(service.isOffline());
    auto settings = *service.getSettings().settings;
    EXPECT_EQ("test", std::get<string>(settings["fakeKey"].value));
    EXPECT_EQ(1, mockHttpSessionAdapter->requests.size());

    service.setOffline();
    EXPECT_TRUE(service.isOffline());

    sleep_for(milliseconds(1500));

    settings = *service.getSettings().settings;
    EXPECT_EQ("test", std::get<string>(settings["fakeKey"].value));
    EXPECT_EQ(1, mockHttpSessionAdapter->requests.size());

    service.setOnline();
    EXPECT_FALSE(service.isOffline());

    settings = *service.getSettings().settings;
    EXPECT_EQ("test", std::get<string>(settings["fakeKey"].value));
    EXPECT_EQ(2, mockHttpSessionAdapter->requests.size());
}

TEST_F(LazyLoadingTest, InitOffline) {
    configcat::Response response = {200, string_format(kTestJsonFormat, R"("test")")};
    mockHttpSessionAdapter->enqueueResponse(response);

    ConfigCatOptions options;
    options.pollingMode = PollingMode::lazyLoad(1);
    options.httpSessionAdapter = mockHttpSessionAdapter;
    options.offline = true;
    auto service = ConfigService(kTestSdkKey, logger, make_shared<Hooks>(), make_shared<NullConfigCache>(), options);

    EXPECT_TRUE(service.isOffline());
    auto settings = service.getSettings().settings;
    EXPECT_TRUE(settings == nullptr);
    EXPECT_EQ(0, mockHttpSessionAdapter->requests.size());

    sleep_for(milliseconds(1500));

    settings = service.getSettings().settings;
    EXPECT_TRUE(settings == nullptr);
    EXPECT_EQ(0, mockHttpSessionAdapter->requests.size());

    service.setOnline();
    EXPECT_FALSE(service.isOffline());

    settings = service.getSettings().settings;
    EXPECT_EQ("test", std::get<string>((*settings)["fakeKey"].value));
    EXPECT_EQ(1, mockHttpSessionAdapter->requests.size());
}
