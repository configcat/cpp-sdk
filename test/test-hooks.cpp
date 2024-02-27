#include <gtest/gtest.h>
#include "mock.h"
#include "utils.h"
#include "configservice.h"
#include "configcat/configcatclient.h"
#include "configcat/configcatoptions.h"
#include "configcat/configcatlogger.h"
#include <chrono>
#include <nlohmann/json.hpp>

using namespace configcat;
using namespace std;
using namespace std::chrono;
using namespace std::this_thread;

class HooksTest : public ::testing::Test {
public:
    shared_ptr<MockHttpSessionAdapter> mockHttpSessionAdapter = make_shared<MockHttpSessionAdapter>();
};

TEST_F(HooksTest, Init) {
    HookCallbacks hookCallbacks;
    auto hooks = make_shared<Hooks>(
        [&]() { hookCallbacks.onClientReady(); },
        [&](std::shared_ptr<configcat::Settings> config) { hookCallbacks.onConfigChanged(config); },
        [&](const EvaluationDetailsBase& details) { hookCallbacks.onFlagEvaluated(details); },
        [&](const string& error) { hookCallbacks.onError(error); }
    );

    ConfigCatOptions options;
    options.pollingMode = PollingMode::manualPoll();
    options.configCache = make_shared<SingleValueCache>(ConfigEntry(
        Config::fromJson(kTestJsonString),
        "test-etag",
        kTestJsonString).serialize()
    );
    options.hooks = hooks;
    auto client = ConfigCatClient::get("test", &options);

    auto value = client->getValue("testStringKey", "");

    auto expectedConfig = Config::fromJson(kTestJsonString);
    expectedConfig->preferences = {};
    expectedConfig->segments = {};
    Config actualConfig;
    actualConfig.settings = hookCallbacks.changedConfig;

    EXPECT_EQ("testValue", value);
    EXPECT_TRUE(hookCallbacks.isReady);
    EXPECT_EQ(1, hookCallbacks.isReadyCallCount);
    EXPECT_EQ(expectedConfig->toJson(), actualConfig.toJson());
    EXPECT_EQ(1, hookCallbacks.changedConfigCallCount);

    EXPECT_EQ("testStringKey", hookCallbacks.evaluationDetails.key);
    EXPECT_EQ("testValue", get<string>(*hookCallbacks.evaluationDetails.value));
    EXPECT_EQ("id", hookCallbacks.evaluationDetails.variationId);
    EXPECT_TRUE(hookCallbacks.evaluationDetails.user == nullptr);
    EXPECT_FALSE(hookCallbacks.evaluationDetails.isDefaultValue);
    EXPECT_TRUE(hookCallbacks.evaluationDetails.error.empty());
    EXPECT_EQ(1, hookCallbacks.evaluationDetailsCallCount);

    EXPECT_TRUE(hookCallbacks.error.empty());
    EXPECT_EQ(0, hookCallbacks.errorCallCount);

    ConfigCatClient::close(client);
}

TEST_F(HooksTest, Subscribe) {
    HookCallbacks hookCallbacks;
    auto hooks = make_shared<Hooks>();
    hooks->addOnClientReady([&]() { hookCallbacks.onClientReady(); });
    hooks->addOnConfigChanged([&](std::shared_ptr<configcat::Settings> config) { hookCallbacks.onConfigChanged(config); });
    hooks->addOnFlagEvaluated([&](const EvaluationDetailsBase& details) { hookCallbacks.onFlagEvaluated(details); });
    hooks->addOnError([&](const string& error) { hookCallbacks.onError(error); });

    ConfigCatOptions options;
    options.pollingMode = PollingMode::manualPoll();
    options.configCache = make_shared<SingleValueCache>(ConfigEntry(
        Config::fromJson(kTestJsonString),
        "test-etag",
        kTestJsonString).serialize()
    );
    options.hooks = hooks;
    auto client = ConfigCatClient::get("test", &options);

    auto value = client->getValue("testStringKey", "");

    auto expectedConfig = Config::fromJson(kTestJsonString);
    expectedConfig->preferences = {};
    expectedConfig->segments = {};
    Config actualConfig;
    actualConfig.settings = hookCallbacks.changedConfig;

    EXPECT_EQ("testValue", value);
    EXPECT_TRUE(hookCallbacks.isReady);
    EXPECT_EQ(1, hookCallbacks.isReadyCallCount);
    EXPECT_EQ(expectedConfig->toJson(), actualConfig.toJson());
    EXPECT_EQ(1, hookCallbacks.changedConfigCallCount);

    EXPECT_EQ("testStringKey", hookCallbacks.evaluationDetails.key);
    EXPECT_EQ("testValue", get<string>(*hookCallbacks.evaluationDetails.value));
    EXPECT_EQ("id", hookCallbacks.evaluationDetails.variationId);
    EXPECT_TRUE(hookCallbacks.evaluationDetails.user == nullptr);
    EXPECT_FALSE(hookCallbacks.evaluationDetails.isDefaultValue);
    EXPECT_TRUE(hookCallbacks.evaluationDetails.error.empty());
    EXPECT_EQ(1, hookCallbacks.evaluationDetailsCallCount);

    EXPECT_TRUE(hookCallbacks.error.empty());
    EXPECT_EQ(0, hookCallbacks.errorCallCount);

    ConfigCatClient::close(client);
}

TEST_F(HooksTest, Evaluation) {
    configcat::Response response = {200, kTestJsonString};
    mockHttpSessionAdapter->enqueueResponse(response);
    HookCallbacks hookCallbacks;

    ConfigCatOptions options;
    options.pollingMode = PollingMode::manualPoll();
    options.httpSessionAdapter = mockHttpSessionAdapter;
    auto client = ConfigCatClient::get("test", &options);

    client->getHooks()->addOnFlagEvaluated([&](const EvaluationDetailsBase& details) { hookCallbacks.onFlagEvaluated(details); });

    client->forceRefresh();

    auto user = make_shared<ConfigCatUser>("test@test1.com");
    auto value = client->getValue("testStringKey", "", user);
    EXPECT_EQ("fake1", value);

    auto& details = hookCallbacks.evaluationDetails;
    EXPECT_EQ("fake1", get<string>(*details.value));
    EXPECT_EQ("testStringKey", details.key);
    EXPECT_EQ("id1", details.variationId);
    EXPECT_FALSE(details.isDefaultValue);
    EXPECT_TRUE(details.error.empty());
    EXPECT_TRUE(details.matchedPercentageOption == std::nullopt);

    auto& rule = details.matchedTargetingRule;
    auto& condition = get<UserCondition>(rule->conditions[0].condition);
    auto& simpleValue = get<SettingValueContainer>(rule->then);
    EXPECT_EQ("fake1", get<string>(simpleValue.value));
    EXPECT_EQ(UserComparator::TextContainsAnyOf, condition.comparator);
    EXPECT_EQ("Identifier", condition.comparisonAttribute);
    EXPECT_EQ("@test1.com", get<vector<string>>(condition.comparisonValue)[0]);
    EXPECT_TRUE(details.user == user);

    auto now =  std::chrono::system_clock::now();
    EXPECT_GT(details.fetchTime, now - std::chrono::seconds(1));
    EXPECT_LE(details.fetchTime, now);

    ConfigCatClient::close(client);
}
