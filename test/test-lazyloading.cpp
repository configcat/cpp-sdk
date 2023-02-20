#include <gtest/gtest.h>
#include "mock.h"
#include "utils.h"
#include "configservice.h"
#include "configcat/configcatoptions.h"
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
    auto service = ConfigService(kTestSdkKey, options);

    auto settings = *service.getSettings();
    EXPECT_EQ("test", std::get<string>(settings["fakeKey"].value));

    settings = *service.getSettings();
    EXPECT_EQ("test", std::get<string>(settings["fakeKey"].value));

    EXPECT_EQ(1, mockHttpSessionAdapter->requests.size());

    // Wait for cache invalidation
    sleep_for(seconds(3));

    settings = *service.getSettings();
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
    auto service = ConfigService(kTestSdkKey, options);

    auto settings = *service.getSettings();
    EXPECT_EQ("test", std::get<string>(settings["fakeKey"].value));

    settings = *service.getSettings();
    EXPECT_EQ("test", std::get<string>(settings["fakeKey"].value));

    EXPECT_EQ(1, mockHttpSessionAdapter->requests.size());

    // Wait for cache invalidation
    sleep_for(seconds(3));

    settings = *service.getSettings();
    EXPECT_EQ("test", std::get<string>(settings["fakeKey"].value));
}

TEST_F(LazyLoadingTest, Cache) {
    auto mockCache = make_shared<InMemoryConfigCache>();

    auto firstJsonString = string_format(kTestJsonFormat, R"("test")");
    configcat::Response firstResponse = {200, firstJsonString};
    mockHttpSessionAdapter->enqueueResponse(firstResponse);
    auto secondJsonString = string_format(kTestJsonFormat, R"("test2")");
    configcat::Response secondResponse = {200, secondJsonString};
    mockHttpSessionAdapter->enqueueResponse(secondResponse);

    ConfigCatOptions options;
    options.pollingMode = PollingMode::lazyLoad(2);
    options.httpSessionAdapter = mockHttpSessionAdapter;
    options.configCache = mockCache;
    auto service = ConfigService(kTestSdkKey, options);

    auto settings = *service.getSettings();
    EXPECT_EQ("test", std::get<string>(settings["fakeKey"].value));

    EXPECT_EQ(1, mockCache->store.size());
    EXPECT_EQ(firstJsonString, mockCache->store.begin()->second);

    // Wait for cache invalidation
    sleep_for(seconds(3));

    settings = *service.getSettings();
    EXPECT_EQ("test2", std::get<string>(settings["fakeKey"].value));

    EXPECT_EQ(1, mockCache->store.size());
    EXPECT_EQ(secondJsonString, mockCache->store.begin()->second);
}
