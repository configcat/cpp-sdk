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

class AutoPollingTest : public ::testing::Test {
public:
    static constexpr char kTestSdkKey[] = "TestSdkKey";
    static constexpr char kTestJsonFormat[] = R"({ "f": { "fakeKey": { "v": %s, "p": [], "r": [] } } })";

    shared_ptr<MockHttpSessionAdapter> mockHttpSessionAdapter = make_shared<MockHttpSessionAdapter>();
};

TEST_F(AutoPollingTest, Get) {
    configcat::Response firstResponse = {200, string_format(kTestJsonFormat, R"("test")")};
    mockHttpSessionAdapter->enqueueResponse(firstResponse);
    configcat::Response secondResponse = {200, string_format(kTestJsonFormat, R"("test2")")};
    mockHttpSessionAdapter->enqueueResponse(secondResponse);

    ConfigCatOptions options;
    options.mode = PollingMode::autoPoll(2);
    options.httpSessionAdapter = mockHttpSessionAdapter;
    auto service = ConfigService(kTestSdkKey, options);

    auto settings = *service.getSettings();
    EXPECT_EQ("test", std::get<string>(settings["fakeKey"].value));

    sleep_for(seconds(3));

    settings = *service.getSettings();
    EXPECT_EQ("test2", std::get<string>(settings["fakeKey"].value));
}

TEST_F(AutoPollingTest, GetFailedRequest) {
    configcat::Response firstResponse = {200, string_format(kTestJsonFormat, R"("test")")};
    mockHttpSessionAdapter->enqueueResponse(firstResponse);
    configcat::Response secondResponse = {500, string_format(kTestJsonFormat, R"("test2")")};
    mockHttpSessionAdapter->enqueueResponse(secondResponse);

    ConfigCatOptions options;
    options.mode = PollingMode::autoPoll(2);
    options.httpSessionAdapter = mockHttpSessionAdapter;
    auto service = ConfigService(kTestSdkKey, options);

    auto settings = *service.getSettings();
    EXPECT_EQ("test", std::get<string>(settings["fakeKey"].value));

    sleep_for(seconds(3));

    settings = *service.getSettings();
    EXPECT_EQ("test", std::get<string>(settings["fakeKey"].value));
}

TEST_F(AutoPollingTest, OnConfigChanged) {
    configcat::Response firstResponse = {200, string_format(kTestJsonFormat, R"("test")")};
    mockHttpSessionAdapter->enqueueResponse(firstResponse);
    configcat::Response secondResponse = {200, string_format(kTestJsonFormat, R"("test2")")};
    mockHttpSessionAdapter->enqueueResponse(secondResponse);

    bool called = false;
    ConfigCatOptions options;
    options.mode = PollingMode::autoPoll(2, 5, [&]{ called = true; });
    options.httpSessionAdapter = mockHttpSessionAdapter;
    auto service = ConfigService(kTestSdkKey, options);

    sleep_for(seconds(1));

    EXPECT_TRUE(called);

    sleep_for(seconds(3));

    auto settings = *service.getSettings();
    EXPECT_EQ("test2", std::get<string>(settings["fakeKey"].value));
}

TEST_F(AutoPollingTest, RequestTimeout) {
    configcat::Response response = {200, string_format(kTestJsonFormat, R"("test")")};
    constexpr int responseDelay = 3;
    mockHttpSessionAdapter->enqueueResponse(response, responseDelay);

    ConfigCatOptions options;
    options.mode = PollingMode::autoPoll(1);
    options.httpSessionAdapter = mockHttpSessionAdapter;
    auto service = ConfigService(kTestSdkKey, options);

    sleep_for(seconds(2));

    EXPECT_EQ(1, mockHttpSessionAdapter->requests.size());

    sleep_for(milliseconds(3500));

    auto settings = service.getSettings();
    ASSERT_TRUE(settings != nullptr);
    EXPECT_EQ("test", std::get<string>(settings->at("fakeKey").value));
}

TEST_F(AutoPollingTest, InitWaitTimeout) {
    configcat::Response response = {200, string_format(kTestJsonFormat, R"("test")")};
    constexpr int responseDelay = 5;
    mockHttpSessionAdapter->enqueueResponse(response, responseDelay);

    auto startTime = std::chrono::steady_clock::now();
    ConfigCatOptions options;
    options.mode = PollingMode::autoPoll(60, 1);
    options.httpSessionAdapter = mockHttpSessionAdapter;
    auto service = ConfigService(kTestSdkKey, options);

    auto settings = service.getSettings();
    EXPECT_EQ(settings, nullptr);

    auto endTime = std::chrono::steady_clock::now();
    auto elapsedTimeInSeconds = std::chrono::duration<double>(endTime - startTime).count();

    EXPECT_TRUE(elapsedTimeInSeconds > 1);
    EXPECT_TRUE(elapsedTimeInSeconds < 2);
}

TEST_F(AutoPollingTest, CancelRequest) {
    configcat::Response response = {200, string_format(kTestJsonFormat, R"("test")")};
    constexpr int responseDelay = 60;
    mockHttpSessionAdapter->enqueueResponse(response, responseDelay);

    auto startTime = std::chrono::steady_clock::now();
    ConfigCatOptions options;
    options.mode = PollingMode::autoPoll(2);
    options.httpSessionAdapter = mockHttpSessionAdapter;
    auto service = ConfigService(kTestSdkKey, options);

    auto settings = service.getSettings();
    EXPECT_EQ(settings, nullptr);

    EXPECT_EQ(1, mockHttpSessionAdapter->responses.size());
}

TEST_F(AutoPollingTest, Cache) {
    auto mockCache = make_shared<InMemoryConfigCache>();

    auto firstJsonString = string_format(kTestJsonFormat, R"("test")");
    configcat::Response firstResponse = {200, firstJsonString};
    mockHttpSessionAdapter->enqueueResponse(firstResponse);
    auto secondJsonString = string_format(kTestJsonFormat, R"("test2")");
    configcat::Response secondResponse = {200, secondJsonString};
    mockHttpSessionAdapter->enqueueResponse(secondResponse);

    ConfigCatOptions options;
    options.mode = PollingMode::autoPoll(2);
    options.httpSessionAdapter = mockHttpSessionAdapter;
    options.cache = mockCache;
    auto service = ConfigService(kTestSdkKey, options);

    auto settings = *service.getSettings();
    EXPECT_EQ("test", std::get<string>(settings["fakeKey"].value));

    EXPECT_EQ(1, mockCache->store.size());
    EXPECT_EQ(firstJsonString, mockCache->store.begin()->second);

    sleep_for(seconds(3));

    settings = *service.getSettings();
    EXPECT_EQ("test2", std::get<string>(settings["fakeKey"].value));

    EXPECT_EQ(1, mockCache->store.size());
    EXPECT_EQ(secondJsonString, mockCache->store.begin()->second);
}
