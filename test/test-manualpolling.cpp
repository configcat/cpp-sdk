#include <gtest/gtest.h>
#include "mock.h"
#include "utils.h"
#include "configservice.h"
#include "configcat/configcatoptions.h"
#include "configcat/configcatlogger.h"
#include "configcat/consolelogger.h"
#include <thread>

using namespace configcat;
using namespace std;
using namespace std::chrono;
using namespace std::this_thread;

class ManualPollingTest : public ::testing::Test {
public:
    static constexpr char kTestSdkKey[] = "TestSdkKey";
    static constexpr char kTestJsonFormat[] = R"({"f":{"fakeKey":{"t":%d,"v":%s}}})";

    shared_ptr<MockHttpSessionAdapter> mockHttpSessionAdapter = make_shared<MockHttpSessionAdapter>();
    shared_ptr<ConfigCatLogger> logger = make_shared<ConfigCatLogger>(make_shared<ConsoleLogger>(), make_shared<Hooks>());
};

TEST_F(ManualPollingTest, Get) {
    configcat::Response firstResponse = {200, string_format(kTestJsonFormat, SettingType::String, R"({"s":"test"})")};
    mockHttpSessionAdapter->enqueueResponse(firstResponse);
    configcat::Response secondResponse = {200, string_format(kTestJsonFormat, SettingType::String, R"({"s":"test2"})")};
    constexpr int secondResponseDelay = 2;
    mockHttpSessionAdapter->enqueueResponse(secondResponse, secondResponseDelay);

    ConfigCatOptions options;
    options.pollingMode = PollingMode::manualPoll();
    options.httpSessionAdapter = mockHttpSessionAdapter;
    auto service = ConfigService(kTestSdkKey, logger, make_shared<Hooks>(), make_shared<NullConfigCache>(), options);

    service.refresh();

    auto settings = *service.getSettings().settings;
    EXPECT_EQ("test", std::get<string>(settings["fakeKey"].value));

    settings = *service.getSettings().settings;
    EXPECT_EQ("test", std::get<string>(settings["fakeKey"].value));

    service.refresh();

    settings = *service.getSettings().settings;
    EXPECT_EQ("test2", std::get<string>(settings["fakeKey"].value));
}

TEST_F(ManualPollingTest, GetFailedRefresh) {
    configcat::Response firstResponse = {200, string_format(kTestJsonFormat, SettingType::String, R"({"s":"test"})")};
    mockHttpSessionAdapter->enqueueResponse(firstResponse);
    configcat::Response secondResponse = {500, string_format(kTestJsonFormat, SettingType::String, R"({"s":"test2"})")};
    mockHttpSessionAdapter->enqueueResponse(secondResponse);

    ConfigCatOptions options;
    options.pollingMode = PollingMode::manualPoll();
    options.httpSessionAdapter = mockHttpSessionAdapter;
    auto service = ConfigService(kTestSdkKey, logger, make_shared<Hooks>(), make_shared<NullConfigCache>(), options);

    service.refresh();

    auto settings = *service.getSettings().settings;
    EXPECT_EQ("test", std::get<string>(settings["fakeKey"].value));

    service.refresh();

    settings = *service.getSettings().settings;
    EXPECT_EQ("test", std::get<string>(settings["fakeKey"].value));
}

TEST_F(ManualPollingTest, Cache) {
    auto mockCache = make_shared<InMemoryConfigCache>();

    configcat::Response firstResponse = {200, string_format(kTestJsonFormat, SettingType::String, R"({"s":"test"})")};
    mockHttpSessionAdapter->enqueueResponse(firstResponse);
    configcat::Response secondResponse = {200, string_format(kTestJsonFormat, SettingType::String, R"({"s":"test2"})")};
    mockHttpSessionAdapter->enqueueResponse(secondResponse);

    ConfigCatOptions options;
    options.pollingMode = PollingMode::manualPoll();
    options.httpSessionAdapter = mockHttpSessionAdapter;
    auto service = ConfigService(kTestSdkKey, logger, make_shared<Hooks>(), mockCache, options);

    service.refresh();

    auto settings = *service.getSettings().settings;
    EXPECT_EQ("test", std::get<string>(settings["fakeKey"].value));

    EXPECT_EQ(1, mockCache->store.size());
    EXPECT_TRUE(contains(mockCache->store.begin()->second, R"({"s":"test"})"));

    service.refresh();

    settings = *service.getSettings().settings;
    EXPECT_EQ("test2", std::get<string>(settings["fakeKey"].value));

    EXPECT_EQ(1, mockCache->store.size());
    EXPECT_TRUE(contains(mockCache->store.begin()->second, R"({"s":"test2"})"));
}

TEST_F(ManualPollingTest, EmptyCacheDoesNotInitiateHTTP) {
    configcat::Response response = {200, string_format(kTestJsonFormat, SettingType::String, R"({"s":"test"})")};
    mockHttpSessionAdapter->enqueueResponse(response);

    ConfigCatOptions options;
    options.pollingMode = PollingMode::manualPoll();
    options.httpSessionAdapter = mockHttpSessionAdapter;
    auto service = ConfigService(kTestSdkKey, logger, make_shared<Hooks>(), make_shared<NullConfigCache>(), options);

    auto settings = service.getSettings().settings;
    EXPECT_EQ(settings, nullptr);

    EXPECT_EQ(0, mockHttpSessionAdapter->requests.size());
}

TEST_F(ManualPollingTest, OnlineOffline) {
    configcat::Response response = {200, string_format(kTestJsonFormat, SettingType::String, R"({"s":"test"})")};
    mockHttpSessionAdapter->enqueueResponse(response);

    ConfigCatOptions options;
    options.pollingMode = PollingMode::manualPoll();
    options.httpSessionAdapter = mockHttpSessionAdapter;
    auto service = ConfigService(kTestSdkKey, logger, make_shared<Hooks>(), make_shared<NullConfigCache>(), options);

    EXPECT_FALSE(service.isOffline());
    EXPECT_TRUE(service.refresh().success);
    auto settings = *service.getSettings().settings;
    EXPECT_EQ("test", std::get<string>(settings["fakeKey"].value));
    EXPECT_EQ(1, mockHttpSessionAdapter->requests.size());

    service.setOffline();
    EXPECT_TRUE(service.isOffline());
    EXPECT_FALSE(service.refresh().success);
    EXPECT_EQ(1, mockHttpSessionAdapter->requests.size());

    service.setOnline();

    EXPECT_FALSE(service.isOffline());
    EXPECT_TRUE(service.refresh().success);
    EXPECT_EQ(2, mockHttpSessionAdapter->requests.size());
}

TEST_F(ManualPollingTest, InitOffline) {
    configcat::Response response = {200, string_format(kTestJsonFormat, SettingType::String, R"({"s":"test"})")};
    mockHttpSessionAdapter->enqueueResponse(response);

    ConfigCatOptions options;
    options.pollingMode = PollingMode::manualPoll();
    options.httpSessionAdapter = mockHttpSessionAdapter;
    options.offline = true;
    auto service = ConfigService(kTestSdkKey, logger, make_shared<Hooks>(), make_shared<NullConfigCache>(), options);

    EXPECT_TRUE(service.isOffline());
    EXPECT_FALSE(service.refresh().success);
    EXPECT_EQ(0, mockHttpSessionAdapter->requests.size());

    service.setOnline();

    EXPECT_FALSE(service.isOffline());
    EXPECT_TRUE(service.refresh().success);
    auto settings = *service.getSettings().settings;
    EXPECT_EQ("test", std::get<string>(settings["fakeKey"].value));
    EXPECT_EQ(1, mockHttpSessionAdapter->requests.size());
}
