#include <gtest/gtest.h>
#include "mock.h"
#include "configcat/configcat.h"
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
        options.pollingMode = PollingMode::manualPoll();
        options.httpSessionAdapter = mockHttpSessionAdapter;

        client = ConfigCatClient::get(kTestSdkKey, &options);
    }

    void TearDown() override {
        ConfigCatClient::closeAll();
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

    ConfigCatClient::closeAll();
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
    options.pollingMode = PollingMode::lazyLoad(120);
    options.httpSessionAdapter = mockHttpSessionAdapter;
    auto client = ConfigCatClient::get(kTestSdkKey, &options);

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
    options.pollingMode = PollingMode::autoPoll(120);
    options.httpSessionAdapter = mockHttpSessionAdapter;
    auto client = ConfigCatClient::get(kTestSdkKey, &options);

    auto value = client->getValue("fakeKey", "");
    EXPECT_EQ("test", value);

    client->forceRefresh();

    value = client->getValue("fakeKey", "");
    EXPECT_EQ("test2", value);
}

TEST_F(ConfigCatClientTest, FailingAutoPoll) {
    mockHttpSessionAdapter->enqueueResponse({500, ""});

    ConfigCatOptions options;
    options.pollingMode = PollingMode::autoPoll(120);
    options.httpSessionAdapter = mockHttpSessionAdapter;
    auto client = ConfigCatClient::get(kTestSdkKey, &options);

    auto value = client->getValue("fakeKey", "");
    EXPECT_EQ("", value);
}

TEST_F(ConfigCatClientTest, FromCacheOnly) {
    auto mockCache = make_shared<InMemoryConfigCache>();
    auto cacheKey = SHA1()(string("cpp_") + ConfigFetcher::kConfigJsonName + "_" + kTestSdkKey);
    auto config = Config::fromJson(string_format(kTestJsonFormat, R"("fake")"));
    auto configEntry = ConfigEntry(config);
    mockCache->write(cacheKey, configEntry.toJson());
    mockHttpSessionAdapter->enqueueResponse({500, ""});

    ConfigCatOptions options;
    options.pollingMode = PollingMode::autoPoll(120);
    options.configCache = mockCache;
    options.httpSessionAdapter = mockHttpSessionAdapter;
    auto client = ConfigCatClient::get(kTestSdkKey, &options);

    auto value = client->getValue("fakeKey", "");
    EXPECT_EQ("fake", value);
}

TEST_F(ConfigCatClientTest, FromCacheOnlyRefresh) {
    auto mockCache = make_shared<InMemoryConfigCache>();
    auto cacheKey = SHA1()(string("cpp_") + ConfigFetcher::kConfigJsonName + "_" + kTestSdkKey);
    auto config = Config::fromJson(string_format(kTestJsonFormat, R"("fake")"));
    auto configEntry = ConfigEntry(config);
    mockCache->write(cacheKey, configEntry.toJson());
    mockHttpSessionAdapter->enqueueResponse({500, ""});

    ConfigCatOptions options;
    options.pollingMode = PollingMode::autoPoll(120);
    options.configCache = mockCache;
    options.httpSessionAdapter = mockHttpSessionAdapter;
    auto client = ConfigCatClient::get(kTestSdkKey, &options);
    client->forceRefresh();

    auto value = client->getValue("fakeKey", "");
    EXPECT_EQ("fake", value);
}

TEST_F(ConfigCatClientTest, FailingAutoPollRefresh) {
    mockHttpSessionAdapter->enqueueResponse({500, ""});

    ConfigCatOptions options;
    options.pollingMode = PollingMode::autoPoll(120);
    options.httpSessionAdapter = mockHttpSessionAdapter;
    auto client = ConfigCatClient::get(kTestSdkKey, &options);

    client->forceRefresh();

    auto value = client->getValue("fakeKey", "");
    EXPECT_EQ("", value);
}

TEST_F(ConfigCatClientTest, FailingExpiringCache) {
    mockHttpSessionAdapter->enqueueResponse({500, ""});

    ConfigCatOptions options;
    options.pollingMode = PollingMode::autoPoll(120);
    options.httpSessionAdapter = mockHttpSessionAdapter;
    auto client = ConfigCatClient::get(kTestSdkKey, &options);

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

TEST_F(ConfigCatClientTest, GetAllValueDetails) {
    SetUp();

    configcat::Response response = {200, kTestJsonString};
    mockHttpSessionAdapter->enqueueResponse(response);
    client->forceRefresh();
    auto allDetails = client->getAllValueDetails();

    auto details_by_key = [&](const std::vector<EvaluationDetails>& all_details, const std::string& key) -> const EvaluationDetails* {
        for (const auto& details : all_details) {
            if (details.key == key) {
                return &details;
            }
        }
        return nullptr;
    };

    EXPECT_EQ(6, allDetails.size());
    auto details = details_by_key(allDetails, "testBoolKey");
    EXPECT_NE(nullptr, details);
    EXPECT_EQ("testBoolKey", details->key);
    EXPECT_EQ(true, get<bool>(details->value));

    details = details_by_key(allDetails, "testStringKey");
    EXPECT_NE(nullptr, details);
    EXPECT_EQ("testStringKey", details->key);
    EXPECT_EQ("testValue", get<string>(details->value));
    EXPECT_EQ("id", details->variationId);

    details = details_by_key(allDetails, "testIntKey");
    EXPECT_NE(nullptr, details);
    EXPECT_EQ("testIntKey", details->key);
    EXPECT_EQ(1, get<int>(details->value));

    details = details_by_key(allDetails, "testDoubleKey");
    EXPECT_NE(nullptr, details);
    EXPECT_EQ("testDoubleKey", details->key);
    EXPECT_EQ(1.1, get<double>(details->value));

    details = details_by_key(allDetails, "key1");
    EXPECT_NE(nullptr, details);
    EXPECT_EQ("key1", details->key);
    EXPECT_EQ(true, get<bool>(details->value));
    EXPECT_EQ("fakeId1", details->variationId);

    details = details_by_key(allDetails, "key2");
    EXPECT_NE(nullptr, details);
    EXPECT_EQ("key2", details->key);
    EXPECT_EQ(false, get<bool>(details->value));
    EXPECT_EQ("fakeId2", details->variationId);
}

TEST_F(ConfigCatClientTest, GetValueDetails) {
    SetUp();

    configcat::Response response = {200, kTestJsonString};
    mockHttpSessionAdapter->enqueueResponse(response);
    client->forceRefresh();

    ConfigCatUser user("test@test1.com");
    auto details = client->getValueDetails("testStringKey", "", &user);

    EXPECT_EQ("fake1", get<string>(details.value));
    EXPECT_EQ("testStringKey", details.key);
    EXPECT_EQ("id1", details.variationId);
    EXPECT_FALSE(details.isDefaultValue);
    EXPECT_TRUE(details.error.empty());
    EXPECT_TRUE(details.matchedEvaluationPercentageRule == nullptr);
    EXPECT_EQ("fake1", get<string>(details.matchedEvaluationRule->value));
    EXPECT_EQ(CONTAINS, details.matchedEvaluationRule->comparator);
    EXPECT_EQ("Identifier", details.matchedEvaluationRule->comparisonAttribute);
    EXPECT_EQ("@test1.com", details.matchedEvaluationRule->comparisonValue);
    EXPECT_EQ(user.toJson(), details.user->toJson());
    std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
    EXPECT_GE(now, details.fetchTime);
    EXPECT_LE(now, details.fetchTime + std::chrono::seconds(1));
}

TEST_F(ConfigCatClientTest, AutoPollUserAgentHeader) {
    configcat::Response response = {200, string_format(kTestJsonFormat, R"("fake")")};
    mockHttpSessionAdapter->enqueueResponse(response);

    ConfigCatOptions options;
    options.pollingMode = PollingMode::autoPoll();
    options.httpSessionAdapter = mockHttpSessionAdapter;
    auto client = ConfigCatClient::get(kTestSdkKey, &options);
    client->forceRefresh();

    auto value = client->getValue("fakeKey", "");
    EXPECT_EQ("fake", value);
    EXPECT_EQ(string("ConfigCat-Cpp/a-") + configcat::version, mockHttpSessionAdapter->requests[0].header["X-ConfigCat-UserAgent"]);
}

TEST_F(ConfigCatClientTest, LazyPollUserAgentHeader) {
    configcat::Response response = {200, string_format(kTestJsonFormat, R"("fake")")};
    mockHttpSessionAdapter->enqueueResponse(response);

    ConfigCatOptions options;
    options.pollingMode = PollingMode::lazyLoad();
    options.httpSessionAdapter = mockHttpSessionAdapter;
    auto client = ConfigCatClient::get(kTestSdkKey, &options);
    client->forceRefresh();

    auto value = client->getValue("fakeKey", "");
    EXPECT_EQ("fake", value);
    EXPECT_EQ(string("ConfigCat-Cpp/l-") + configcat::version, mockHttpSessionAdapter->requests[0].header["X-ConfigCat-UserAgent"]);
}

TEST_F(ConfigCatClientTest, ManualPollUserAgentHeader) {
    configcat::Response response = {200, string_format(kTestJsonFormat, R"("fake")")};
    mockHttpSessionAdapter->enqueueResponse(response);

    ConfigCatOptions options;
    options.pollingMode = PollingMode::manualPoll();
    options.httpSessionAdapter = mockHttpSessionAdapter;
    auto client = ConfigCatClient::get(kTestSdkKey, &options);
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
    options.pollingMode = PollingMode::autoPoll(2);
    options.httpSessionAdapter = mockHttpSessionAdapter;
    auto client = ConfigCatClient::get(kTestSdkKey, &options);

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

#ifndef __APPLE__
// TODO: This test is broken on GitHub macos-latest os.
TEST_F(ConfigCatClientTest, Concurrency_OngoingFetchDoesNotBlockGetValue) {
    configcat::Response firstResponse = {200, string_format(kTestJsonFormat, R"("fake")")};
    mockHttpSessionAdapter->enqueueResponse(firstResponse);
    configcat::Response secondResponse = {200, string_format(kTestJsonFormat, R"("fake2")")};
    constexpr int responseDelay = 3;
    mockHttpSessionAdapter->enqueueResponse(secondResponse, responseDelay);

    ConfigCatOptions options;
    options.pollingMode = PollingMode::autoPoll(1);
    options.httpSessionAdapter = mockHttpSessionAdapter;
    auto client = ConfigCatClient::get(kTestSdkKey, &options);

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

    sleep_for(chrono::milliseconds(4500));

    value = client->getValue("fakeKey", "");
    EXPECT_EQ("fake2", value);

    t.join();
    EXPECT_EQ(2, mockHttpSessionAdapter->requests.size());
}
#endif

TEST_F(ConfigCatClientTest, GetValueTypeTest) {
    SetUp();

    bool boolValue = client->getValue("", false);
    EXPECT_EQ(boolValue, false);

    string stringValue = client->getValue("", "str");
    EXPECT_EQ(stringValue, "str");

    stringValue = client->getValue("", "str"s);
    EXPECT_EQ(stringValue, "str");

    char defaultChars[] = "str";
    stringValue = client->getValue("", defaultChars);
    EXPECT_EQ(stringValue, "str");

    stringValue = client->getValue("", string("str"));
    EXPECT_EQ(stringValue, "str");

    int intValue = client->getValue("", 42);
    EXPECT_EQ(intValue, 42);

    double doubleValue = client->getValue("", 42.0);
    EXPECT_EQ(doubleValue, 42.0);
}

TEST_F(ConfigCatClientTest, GetValueWithKeyNotFound) {
    SetUp();

    configcat::Response response = {200, string_format(kTestJsonFormat, "43")};
    mockHttpSessionAdapter->enqueueResponse(response);
    client->forceRefresh();

    auto value = client->getValue("nonexisting", 10);
    EXPECT_EQ(10, value);

    ConfigCatUser* user = nullptr;
    shared_ptr<Value> valuePtr = client->getValue("nonexisting", user);
    EXPECT_EQ(nullptr, valuePtr);
}

TEST_F(ConfigCatClientTest, DefaultUserGetValue) {
    SetUp();

    configcat::Response response = {200, kTestJsonString};
    mockHttpSessionAdapter->enqueueResponse(response);
    client->forceRefresh();

    auto user1 = make_shared<ConfigCatUser>("test@test1.com");
    auto user2 = make_shared<ConfigCatUser>("test@test2.com");

    client->setDefaultUser(user1);
    EXPECT_EQ("fake1", client->getValue("testStringKey", ""));
    EXPECT_EQ("fake2", client->getValue("testStringKey", "", user2.get()));

    client->clearDefaultUser();
    EXPECT_EQ("testValue", client->getValue("testStringKey", ""));
}

TEST_F(ConfigCatClientTest, DefaultUserGetAllValues) {
    SetUp();

    configcat::Response response = {200, kTestJsonString};
    mockHttpSessionAdapter->enqueueResponse(response);
    client->forceRefresh();

    auto user1 = make_shared<ConfigCatUser>("test@test1.com");
    auto user2 = make_shared<ConfigCatUser>("test@test2.com");

    client->setDefaultUser(user1);
    auto allValues = client->getAllValues();
    EXPECT_EQ(6, allValues.size());
    EXPECT_EQ(true, get<bool>(allValues["testBoolKey"]));
    EXPECT_EQ("fake1", get<string>(allValues["testStringKey"]));
    EXPECT_EQ(1, get<int>(allValues["testIntKey"]));
    EXPECT_EQ(1.1, get<double>(allValues["testDoubleKey"]));
    EXPECT_TRUE(get<bool>(allValues["key1"]));
    EXPECT_FALSE(get<bool>(allValues["key2"]));

    allValues = client->getAllValues(user2.get());
    EXPECT_EQ(6, allValues.size());
    EXPECT_EQ(true, get<bool>(allValues["testBoolKey"]));
    EXPECT_EQ("fake2", get<string>(allValues["testStringKey"]));
    EXPECT_EQ(1, get<int>(allValues["testIntKey"]));
    EXPECT_EQ(1.1, get<double>(allValues["testDoubleKey"]));
    EXPECT_TRUE(get<bool>(allValues["key1"]));
    EXPECT_FALSE(get<bool>(allValues["key2"]));

    client->clearDefaultUser();
    allValues = client->getAllValues();
    EXPECT_EQ(6, allValues.size());
    EXPECT_EQ(true, get<bool>(allValues["testBoolKey"]));
    EXPECT_EQ("testValue", get<string>(allValues["testStringKey"]));
    EXPECT_EQ(1, get<int>(allValues["testIntKey"]));
    EXPECT_EQ(1.1, get<double>(allValues["testDoubleKey"]));
    EXPECT_TRUE(get<bool>(allValues["key1"]));
    EXPECT_FALSE(get<bool>(allValues["key2"]));
}

TEST_F(ConfigCatClientTest, OnlineOffline) {
    SetUp();

    configcat::Response response = {200, kTestJsonString};
    mockHttpSessionAdapter->enqueueResponse(response);
    mockHttpSessionAdapter->enqueueResponse(response);

    EXPECT_FALSE(client->isOffline());

    client->forceRefresh();

    EXPECT_EQ(1, mockHttpSessionAdapter->requests.size());

    client->setOffline();
    EXPECT_TRUE(client->isOffline());

    client->forceRefresh();

    EXPECT_EQ(1, mockHttpSessionAdapter->requests.size());

    client->setOnline();
    EXPECT_FALSE(client->isOffline());

    client->forceRefresh();

    EXPECT_EQ(2, mockHttpSessionAdapter->requests.size());
}

TEST_F(ConfigCatClientTest, InitOffline) {
    configcat::Response response = {200, kTestJsonString};
    mockHttpSessionAdapter->enqueueResponse(response);
    mockHttpSessionAdapter->enqueueResponse(response);

    ConfigCatOptions options;
    options.pollingMode = PollingMode::manualPoll();
    options.httpSessionAdapter = mockHttpSessionAdapter;
    options.offline = true;
    client = ConfigCatClient::get(kTestSdkKey, &options);

    EXPECT_TRUE(client->isOffline());

    client->forceRefresh();

    EXPECT_EQ(0, mockHttpSessionAdapter->requests.size());

    client->setOnline();
    EXPECT_FALSE(client->isOffline());

    client->forceRefresh();

    EXPECT_EQ(1, mockHttpSessionAdapter->requests.size());
}

