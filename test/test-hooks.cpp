#include <gtest/gtest.h>
#include "mock.h"
#include "utils.h"
#include "configservice.h"
#include "configcat/configcatclient.h"
#include "configcat/configcatoptions.h"
#include "configcat/configcatlogger.h"
#include "configcat/consolelogger.h"
#include <thread>
#include <chrono>

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
        [&](const EvaluationDetails& details) { hookCallbacks.onFlagEvaluated(details); },
        [&](const string& error) { hookCallbacks.onError(error); }
    );

    auto config = Config::fromJson(kTestJsonString);

    ConfigCatOptions options;
    options.pollingMode = PollingMode::manualPoll();
    options.configCache = make_shared<SingleValueCache>(R"({"config":)"s + kTestJsonString + R"(,"etag":"test-etag"})");
    options.hooks = hooks;
    auto client = ConfigCatClient::get("test", &options);

    auto value = client->getValue("testStringKey", "");

    EXPECT_EQ("testValue", value);
    EXPECT_TRUE(hookCallbacks.isReady);
    EXPECT_EQ(1, hookCallbacks.isReadyCallCount);
    EXPECT_EQ(*config->entries, *hookCallbacks.changedConfig);
    EXPECT_EQ(1, hookCallbacks.changedConfigCallCount);

    EXPECT_EQ("testStringKey", hookCallbacks.evaluationDetails.key);
    EXPECT_EQ("testValue", get<string>(hookCallbacks.evaluationDetails.value));
    EXPECT_EQ("id", hookCallbacks.evaluationDetails.variationId);
    EXPECT_TRUE(hookCallbacks.evaluationDetails.user == nullptr);
    EXPECT_FALSE(hookCallbacks.evaluationDetails.isDefaultValue);
    EXPECT_TRUE(hookCallbacks.evaluationDetails.error.empty());
    EXPECT_EQ(1, hookCallbacks.evaluationDetailsCallCount);

    EXPECT_TRUE(hookCallbacks.error.empty());
    EXPECT_EQ(0, hookCallbacks.errorCallCount);
}

TEST_F(HooksTest, Subscribe) {
    HookCallbacks hookCallbacks;
    auto hooks = make_shared<Hooks>();
    hooks->addOnClientReady([&]() { hookCallbacks.onClientReady(); });
    hooks->addOnConfigChanged([&](std::shared_ptr<configcat::Settings> config) { hookCallbacks.onConfigChanged(config); });
    hooks->addOnFlagEvaluated([&](const EvaluationDetails& details) { hookCallbacks.onFlagEvaluated(details); });
    hooks->addOnError([&](const string& error) { hookCallbacks.onError(error); });

    auto config = Config::fromJson(kTestJsonString);

    ConfigCatOptions options;
    options.pollingMode = PollingMode::manualPoll();
    options.configCache = make_shared<SingleValueCache>(R"({"config":)"s + kTestJsonString + R"(,"etag":"test-etag"})");
    options.hooks = hooks;
    auto client = ConfigCatClient::get("test", &options);

    auto value = client->getValue("testStringKey", "");

    EXPECT_EQ("testValue", value);
    EXPECT_TRUE(hookCallbacks.isReady);
    EXPECT_EQ(1, hookCallbacks.isReadyCallCount);
    EXPECT_EQ(*config->entries, *hookCallbacks.changedConfig);
    EXPECT_EQ(1, hookCallbacks.changedConfigCallCount);

    EXPECT_EQ("testStringKey", hookCallbacks.evaluationDetails.key);
    EXPECT_EQ("testValue", get<string>(hookCallbacks.evaluationDetails.value));
    EXPECT_EQ("id", hookCallbacks.evaluationDetails.variationId);
    EXPECT_TRUE(hookCallbacks.evaluationDetails.user == nullptr);
    EXPECT_FALSE(hookCallbacks.evaluationDetails.isDefaultValue);
    EXPECT_TRUE(hookCallbacks.evaluationDetails.error.empty());
    EXPECT_EQ(1, hookCallbacks.evaluationDetailsCallCount);

    EXPECT_TRUE(hookCallbacks.error.empty());
    EXPECT_EQ(0, hookCallbacks.errorCallCount);
}

TEST_F(HooksTest, Evaluation) {
    configcat::Response response = {200, kTestJsonString};
    mockHttpSessionAdapter->enqueueResponse(response);
    HookCallbacks hookCallbacks;

    ConfigCatOptions options;
    options.pollingMode = PollingMode::manualPoll();
    options.httpSessionAdapter = mockHttpSessionAdapter;
    auto client = ConfigCatClient::get("test", &options);

    client->getHooks()->addOnFlagEvaluated([&](const EvaluationDetails& details) { hookCallbacks.onFlagEvaluated(details); });

    client->forceRefresh();

    ConfigCatUser user("test@test1.com");
    auto value = client->getValue("testStringKey", "", &user);
    EXPECT_EQ("fake1", value);

    auto& details = hookCallbacks.evaluationDetails;
    EXPECT_EQ("fake1", get<string>(details.value));
    EXPECT_EQ("testStringKey", details.key);
    EXPECT_EQ("id1", details.variationId);
    EXPECT_FALSE(details.isDefaultValue);
    EXPECT_TRUE(details.error.empty());
    EXPECT_TRUE(details.matchedEvaluationPercentageRule == nullptr);

    auto rule = details.matchedEvaluationRule;
    EXPECT_EQ("fake1", get<string>(rule->value));
    EXPECT_EQ(Comparator::CONTAINS, rule->comparator);
    EXPECT_EQ("Identifier", rule->comparisonAttribute);
    EXPECT_EQ("@test1.com", rule->comparisonValue);
    EXPECT_TRUE(details.user == &user);

    auto now =  std::chrono::system_clock::now();
    EXPECT_GT(details.fetchTime, now - std::chrono::seconds(1));
    EXPECT_LE(details.fetchTime, now);
}
