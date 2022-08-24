#include <gtest/gtest.h>
#include "mock.h"
#include "configcat/configcatclient.h"
#include "configfetcher.h"
#include "utils.h"
#include <hash-library/sha1.h>

using namespace configcat;
using namespace std;
using namespace std::this_thread;

class ConfigCatClientTest : public ::testing::Test {
public:
    static constexpr char kTestSdkKey[] = "TestSdkKey";
    static constexpr char kTestJsonFormat[] = R"({ "f": { "fakeKey": { "v": %s, "p": [], "r": [] } } })";
    static constexpr char kTestJsonMultiple[] = R"({ "f": { "key1": { "v": true, "i": "fakeId1", "p": [], "r": [] }, "key2": { "v": false, "i": "fakeId2", "p": [], "r": [] } } })";

    ConfigCatClient* client = nullptr;
    shared_ptr<MockHttpSessionAdapter> mockHttpSessionAdapter = make_shared<MockHttpSessionAdapter>();

    void SetUp(const std::string& sdkKey = kTestSdkKey) {
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
    SetUp();

    auto client2 = ConfigCatClient::get(kTestSdkKey);
    EXPECT_TRUE(client2 == client);
}

TEST_F(ConfigCatClientTest, EnsureCloseWorks) {
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

TEST_F(ConfigCatClientTest, GetIntValue) {
    SetUp();

    configcat::Response response = {200, string_format(kTestJsonFormat, "43")};
    mockHttpSessionAdapter->enqueueResponse(response);
    client->forceRefresh();
    auto value = client->getValue("fakeKey", 10);

    EXPECT_EQ(43, value);
}

TEST_F(ConfigCatClientTest, GetIntValueFailed) {
    SetUp();

    configcat::Response response = {200, string_format(kTestJsonFormat, R"("fake")")};
    mockHttpSessionAdapter->enqueueResponse(response);
    client->forceRefresh();
    auto value = client->getValue("fakeKey", 10);

    EXPECT_EQ(10, value);
}

TEST_F(ConfigCatClientTest, GetIntValueFailedInvalidJson) {
    SetUp();

    configcat::Response response = {200, "{"};
    mockHttpSessionAdapter->enqueueResponse(response);
    client->forceRefresh();
    auto value = client->getValue("fakeKey", 10);

    EXPECT_EQ(10, value);
}

TEST_F(ConfigCatClientTest, GetIntValueFailedPartialJson) {
    SetUp();

    configcat::Response responseWithoutValue = {200, R"({ "f": { "fakeKey": { "p": [], "r": [] } } })"};
    mockHttpSessionAdapter->enqueueResponse(responseWithoutValue);
    client->forceRefresh();
    auto value = client->getValue("fakeKey", 10);

    EXPECT_EQ(10, value);
}

TEST_F(ConfigCatClientTest, GetIntValueFailedNullValueJson) {
    SetUp();

    configcat::Response responseWithoutValue = {200, R"({ "f": { "fakeKey": { "v": null, "p": [], "r": [] } } })"};
    mockHttpSessionAdapter->enqueueResponse(responseWithoutValue);
    client->forceRefresh();
    auto value = client->getValue("fakeKey", 10);

    EXPECT_EQ(10, value);
}

TEST_F(ConfigCatClientTest, GetStringValue) {
    SetUp();

    configcat::Response response = {200, string_format(kTestJsonFormat, R"("fake")")};
    mockHttpSessionAdapter->enqueueResponse(response);
    client->forceRefresh();
    auto value = client->getValue("fakeKey", "default");

    EXPECT_EQ("fake", value);
}

TEST_F(ConfigCatClientTest, GetStringValueFailed) {
    SetUp();

    configcat::Response response = {200, string_format(kTestJsonFormat, "33")};
    mockHttpSessionAdapter->enqueueResponse(response);
    client->forceRefresh();
    auto value = client->getValue("fakeKey", "default");

    EXPECT_EQ("default", value);
}

TEST_F(ConfigCatClientTest, GetDoubleValue) {
    SetUp();

    configcat::Response response = {200, string_format(kTestJsonFormat, "43.56")};
    mockHttpSessionAdapter->enqueueResponse(response);
    client->forceRefresh();
    auto value = client->getValue("fakeKey", 3.14);

    EXPECT_EQ(43.56, value);
}

TEST_F(ConfigCatClientTest, GetDoubleValueFailed) {
    SetUp();

    configcat::Response response = {200, string_format(kTestJsonFormat, R"("fake")")};
    mockHttpSessionAdapter->enqueueResponse(response);
    client->forceRefresh();
    auto value = client->getValue("fakeKey", 3.14);

    EXPECT_EQ(3.14, value);
}

TEST_F(ConfigCatClientTest, GetBoolValue) {
    SetUp();

    configcat::Response response = {200, string_format(kTestJsonFormat, "true")};
    mockHttpSessionAdapter->enqueueResponse(response);
    client->forceRefresh();
    auto value = client->getValue("fakeKey", false);

    EXPECT_TRUE(value);
}

TEST_F(ConfigCatClientTest, GetBoolValueFailed) {
    SetUp();

    configcat::Response response = {200, string_format(kTestJsonFormat, R"("fake")")};
    mockHttpSessionAdapter->enqueueResponse(response);
    client->forceRefresh();
    auto value = client->getValue("fakeKey", false);

    EXPECT_FALSE(value);
}

TEST_F(ConfigCatClientTest, GetLatestOnFail) {
    SetUp();

    configcat::Response firstResponse = {200, string_format(kTestJsonFormat, R"(55)")};
    mockHttpSessionAdapter->enqueueResponse(firstResponse);
    configcat::Response secondResponse = {500, ""};
    mockHttpSessionAdapter->enqueueResponse(secondResponse);
    client->forceRefresh();

    auto value = client->getValue("fakeKey", 0);
    EXPECT_EQ(55, value);

    client->forceRefresh();

    value = client->getValue("fakeKey", 0);
    EXPECT_EQ(55, value);
}

TEST_F(ConfigCatClientTest, ForceRefreshLazy) {
    configcat::Response firstResponse = {200, string_format(kTestJsonFormat, R"("test")")};
    mockHttpSessionAdapter->enqueueResponse(firstResponse);
    configcat::Response secondResponse = {200, string_format(kTestJsonFormat, R"("test2")")};
    mockHttpSessionAdapter->enqueueResponse(secondResponse);

    ConfigCatOptions options;
    options.mode = PollingMode::lazyLoad(120);
    options.httpSessionAdapter = mockHttpSessionAdapter;
    auto client = ConfigCatClient::get(kTestSdkKey, options);

    auto value = client->getValue("fakeKey", "");
    EXPECT_EQ("test", value);

    client->forceRefresh();

    value = client->getValue("fakeKey", "");
    EXPECT_EQ("test2", value);
}

TEST_F(ConfigCatClientTest, ForceRefreshAuto) {
    configcat::Response firstResponse = {200, string_format(kTestJsonFormat, R"("test")")};
    mockHttpSessionAdapter->enqueueResponse(firstResponse);
    configcat::Response secondResponse = {200, string_format(kTestJsonFormat, R"("test2")")};
    mockHttpSessionAdapter->enqueueResponse(secondResponse);

    ConfigCatOptions options;
    options.mode = PollingMode::autoPoll(120);
    options.httpSessionAdapter = mockHttpSessionAdapter;
    auto client = ConfigCatClient::get(kTestSdkKey, options);

    auto value = client->getValue("fakeKey", "");
    EXPECT_EQ("test", value);

    client->forceRefresh();

    value = client->getValue("fakeKey", "");
    EXPECT_EQ("test2", value);
}

TEST_F(ConfigCatClientTest, FailingAutoPoll) {
    mockHttpSessionAdapter->enqueueResponse({500, ""});

    ConfigCatOptions options;
    options.mode = PollingMode::autoPoll(120);
    options.httpSessionAdapter = mockHttpSessionAdapter;
    auto client = ConfigCatClient::get(kTestSdkKey, options);

    auto value = client->getValue("fakeKey", "");
    EXPECT_EQ("", value);
}

TEST_F(ConfigCatClientTest, FromCacheOnly) {
    auto mockCache = make_shared<InMemoryConfigCache>();
    auto cacheKey = SHA1()(string("cpp_") + ConfigFetcher::kConfigJsonName + "_" + kTestSdkKey);
    mockCache->write(cacheKey, string_format(kTestJsonFormat, R"("fake")"));
    mockHttpSessionAdapter->enqueueResponse({500, ""});

    ConfigCatOptions options;
    options.mode = PollingMode::autoPoll(120);
    options.cache = mockCache;
    options.httpSessionAdapter = mockHttpSessionAdapter;
    auto client = ConfigCatClient::get(kTestSdkKey, options);

    auto value = client->getValue("fakeKey", "");
    EXPECT_EQ("fake", value);
}

TEST_F(ConfigCatClientTest, FromCacheOnlyRefresh) {
    auto mockCache = make_shared<InMemoryConfigCache>();
    auto cacheKey = SHA1()(string("cpp_") + ConfigFetcher::kConfigJsonName + "_" + kTestSdkKey);
    mockCache->write(cacheKey, string_format(kTestJsonFormat, R"("fake")"));
    mockHttpSessionAdapter->enqueueResponse({500, ""});

    ConfigCatOptions options;
    options.mode = PollingMode::autoPoll(120);
    options.cache = mockCache;
    options.httpSessionAdapter = mockHttpSessionAdapter;
    auto client = ConfigCatClient::get(kTestSdkKey, options);
    client->forceRefresh();

    auto value = client->getValue("fakeKey", "");
    EXPECT_EQ("fake", value);
}

TEST_F(ConfigCatClientTest, FailingAutoPollRefresh) {
    mockHttpSessionAdapter->enqueueResponse({500, ""});

    ConfigCatOptions options;
    options.mode = PollingMode::autoPoll(120);
    options.httpSessionAdapter = mockHttpSessionAdapter;
    auto client = ConfigCatClient::get(kTestSdkKey, options);

    client->forceRefresh();

    auto value = client->getValue("fakeKey", "");
    EXPECT_EQ("", value);
}

TEST_F(ConfigCatClientTest, FailingExpiringCache) {
    mockHttpSessionAdapter->enqueueResponse({500, ""});

    ConfigCatOptions options;
    options.mode = PollingMode::autoPoll(120);
    options.httpSessionAdapter = mockHttpSessionAdapter;
    auto client = ConfigCatClient::get(kTestSdkKey, options);

    auto value = client->getValue("fakeKey", "");
    EXPECT_EQ("", value);
}

TEST_F(ConfigCatClientTest, GetAllKeys) {
    SetUp();

    configcat::Response response = {200, kTestJsonMultiple};
    mockHttpSessionAdapter->enqueueResponse(response);
    client->forceRefresh();
    auto keys = client->getAllKeys();

    EXPECT_EQ(2, keys.size());
    EXPECT_TRUE(std::find(keys.begin(), keys.end(), "key1") != keys.end());
    EXPECT_TRUE(std::find(keys.begin(), keys.end(), "key2") != keys.end());
}

TEST_F(ConfigCatClientTest, GetAllValues) {
    SetUp();

    configcat::Response response = {200, kTestJsonMultiple};
    mockHttpSessionAdapter->enqueueResponse(response);
    client->forceRefresh();
    auto allValues = client->getAllValues();

    EXPECT_EQ(2, allValues.size());
    EXPECT_EQ(true, std::get<bool>(allValues.at("key1")));
    EXPECT_EQ(false, std::get<bool>(allValues.at("key2")));
}

TEST_F(ConfigCatClientTest, AutoPollUserAgentHeader) {
    configcat::Response response = {200, string_format(kTestJsonFormat, R"("fake")")};
    mockHttpSessionAdapter->enqueueResponse(response);

    ConfigCatOptions options;
    options.mode = PollingMode::autoPoll();
    options.httpSessionAdapter = mockHttpSessionAdapter;
    auto client = ConfigCatClient::get(kTestSdkKey, options);
    client->forceRefresh();

    auto value = client->getValue("fakeKey", "");
    EXPECT_EQ("fake", value);
    EXPECT_EQ(string("ConfigCat-Cpp/a-") + configcat::version, mockHttpSessionAdapter->requests[0].header["X-ConfigCat-UserAgent"]);
}

TEST_F(ConfigCatClientTest, LazyPollUserAgentHeader) {
    configcat::Response response = {200, string_format(kTestJsonFormat, R"("fake")")};
    mockHttpSessionAdapter->enqueueResponse(response);

    ConfigCatOptions options;
    options.mode = PollingMode::lazyLoad();
    options.httpSessionAdapter = mockHttpSessionAdapter;
    auto client = ConfigCatClient::get(kTestSdkKey, options);
    client->forceRefresh();

    auto value = client->getValue("fakeKey", "");
    EXPECT_EQ("fake", value);
    EXPECT_EQ(string("ConfigCat-Cpp/l-") + configcat::version, mockHttpSessionAdapter->requests[0].header["X-ConfigCat-UserAgent"]);
}

TEST_F(ConfigCatClientTest, ManualPollUserAgentHeader) {
    configcat::Response response = {200, string_format(kTestJsonFormat, R"("fake")")};
    mockHttpSessionAdapter->enqueueResponse(response);

    ConfigCatOptions options;
    options.mode = PollingMode::manualPoll();
    options.httpSessionAdapter = mockHttpSessionAdapter;
    auto client = ConfigCatClient::get(kTestSdkKey, options);
    client->forceRefresh();

    auto value = client->getValue("fakeKey", "");
    EXPECT_EQ("fake", value);
    EXPECT_EQ(string("ConfigCat-Cpp/m-") + configcat::version, mockHttpSessionAdapter->requests[0].header["X-ConfigCat-UserAgent"]);
}

TEST_F(ConfigCatClientTest, Concurrency_DoNotStartNewFetchIfThereIsAnOngoingFetch) {
    configcat::Response response = {200, string_format(kTestJsonFormat, R"("fake")")};
    constexpr int responseDelay = 1;
    mockHttpSessionAdapter->enqueueResponse(response, responseDelay);

    ConfigCatOptions options;
    options.mode = PollingMode::autoPoll(2);
    options.httpSessionAdapter = mockHttpSessionAdapter;
    auto client = ConfigCatClient::get(kTestSdkKey, options);

    thread t([&]() {
        sleep_for(chrono::milliseconds(500));
        client->forceRefresh();

        auto value = client->getValue("fakeKey", "");
        EXPECT_EQ("fake", value);
    });

    auto value = client->getValue("fakeKey", "");
    EXPECT_EQ("fake", value);

    t.join();

    EXPECT_EQ(1, mockHttpSessionAdapter->requests.size());
}


// test
std::string get_time()
{
    using namespace std::chrono;
    auto timepoint = system_clock::now();
    auto coarse = system_clock::to_time_t(timepoint);
    auto fine = time_point_cast<std::chrono::milliseconds>(timepoint);

    char buffer[sizeof "9999-12-31 23:59:59.999"];
    std::snprintf(buffer + std::strftime(buffer, sizeof buffer - 3,
                                         "%F %T.", std::localtime(&coarse)),
                  4, "%03lu", fine.time_since_epoch().count() % 1000);

    return buffer;
}
// test

#if 0
TEST_F(ConfigCatClientTest, Concurrency_OngoingFetchDoesNotBlockGetValue) {
    // test
    std::cout << "Concurrency_OngoingFetchDoesNotBlockGetValue() - time: " << get_time() << " thread_id: " << this_thread::get_id() << endl;
    // test

    configcat::Response firstResponse = {200, string_format(kTestJsonFormat, R"("fake")")};
    mockHttpSessionAdapter->enqueueResponse(firstResponse);
    configcat::Response secondResponse = {200, string_format(kTestJsonFormat, R"("fake2")")};
    constexpr int responseDelay = 2;
    mockHttpSessionAdapter->enqueueResponse(secondResponse, responseDelay);

    ConfigCatOptions options;
    options.mode = PollingMode::autoPoll(1);
    options.httpSessionAdapter = mockHttpSessionAdapter;
    auto client = ConfigCatClient::get(kTestSdkKey, options);

    thread t([&]() {
        sleep_for(chrono::milliseconds(1500));

        auto startTime = chrono::steady_clock::now();

        auto value = client->getValue("fakeKey", "");
        EXPECT_EQ("fake", value);

        auto endTime = chrono::steady_clock::now();
        auto elapsedTimeInSeconds = chrono::duration<double>(endTime - startTime).count();
        EXPECT_LT(elapsedTimeInSeconds, 0.1);
    });

    auto value = client->getValue("fakeKey", "");
    EXPECT_EQ("fake", value);

//    sleep_for(chrono::milliseconds(4000)); // 3500 (4500 -> 6000)
//
//    std::cout << "value = client->getValue(\"fakeKey\", \"\") - time: " << get_time() << " thread_id: " << this_thread::get_id() << endl;
//    value = client->getValue("fakeKey", "");
//    std::cout << "EXPECT_EQ(\"fake2\", value) - time: " << get_time() << " thread_id: " << this_thread::get_id() << endl;
//    EXPECT_EQ("fake2", value);

    t.join();
//    EXPECT_EQ(1, mockHttpSessionAdapter->requests.size());
}
#endif










TEST_F(ConfigCatClientTest, GetValueTest) {
    SetUp();

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
