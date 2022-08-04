#include <gtest/gtest.h>
#include "configcat/configfetcher.h"
#include "configcat/configcatoptions.h"
#include "configcat/configjsoncache.h"
#include "mock.h"
#include "utils.h"
#include "configcat/config.h"

using namespace configcat;
using namespace std;

class ConfigFetcherTest : public ::testing::Test {
public:
    static constexpr char kTestSdkKey[] = "TestSdkKey";
    static constexpr char kCustomCdnUrl[] = "https://custom-cdn.configcat.com";
    unique_ptr<ConfigJsonCache> jsonCache;
    unique_ptr<ConfigFetcher> fetcher;
    shared_ptr<MockHttpSessionAdapter> mockHttpSessionAdapter = make_shared<MockHttpSessionAdapter>();

    void SetUp(const std::string& baseUrl = "", const std::string& sdkKey = kTestSdkKey) {
        // Using of designated initializers requires at least '/std:c++20'
        ConfigCatOptions options;
        options.mode = PollingMode::manualPoll();
        options.httpSessionAdapter = mockHttpSessionAdapter;
        options.baseUrl = baseUrl;

        jsonCache = make_unique<ConfigJsonCache>(sdkKey);
        fetcher = make_unique<ConfigFetcher>("TestSdkKey", "m", *jsonCache, options);
    }

    void TearDown() override {
    }

    string createTestJson(const string& url, RedirectMode redirectMode) {
        return string_format(R"({ "p": { "u": "%s", "r": %d }, "f": {} })", url.c_str(), redirectMode);
    }
};

TEST_F(ConfigFetcherTest, DataGovernance_ShouldStayOnGivenUrl) {
    SetUp();

    configcat::Response response = {200, createTestJson(ConfigFetcher::kGlobalBaseUrl, NoRedirect)};
    mockHttpSessionAdapter->enqueueResponse(response);
    auto fetchResponse = fetcher->fetchConfiguration();

    ASSERT_TRUE(fetchResponse.config != Config::empty);
    EXPECT_EQ(fetchResponse.config->preferences->url, ConfigFetcher::kGlobalBaseUrl);
    EXPECT_EQ(fetchResponse.config->preferences->redirect, NoRedirect);
}

TEST_F(ConfigFetcherTest, DataGovernance_ShouldStayOnSameUrl) {
    SetUp();

    configcat::Response response = {200, createTestJson(ConfigFetcher::kGlobalBaseUrl, ShouldRedirect)};
    mockHttpSessionAdapter->enqueueResponse(response);
    auto fetchResponse = fetcher->fetchConfiguration();

    ASSERT_TRUE(fetchResponse.config != Config::empty);
    EXPECT_EQ(fetchResponse.config->preferences->url, ConfigFetcher::kGlobalBaseUrl);
    EXPECT_EQ(fetchResponse.config->preferences->redirect, ShouldRedirect);
}

TEST_F(ConfigFetcherTest, DataGovernance_ShouldStayOnSameUrlEvenWithForce) {
    SetUp();

    configcat::Response response = {200, createTestJson(ConfigFetcher::kGlobalBaseUrl, ForceRedirect)};
    mockHttpSessionAdapter->enqueueResponse(response);
    auto fetchResponse = fetcher->fetchConfiguration();

    ASSERT_TRUE(fetchResponse.config != Config::empty);
    EXPECT_EQ(fetchResponse.config->preferences->url, ConfigFetcher::kGlobalBaseUrl);
    EXPECT_EQ(fetchResponse.config->preferences->redirect, ForceRedirect);
}

TEST_F(ConfigFetcherTest, DataGovernance_ShouldRedirectToAnotherServer) {
    SetUp();

    configcat::Response firstResponse = {200, createTestJson(ConfigFetcher::kEuOnlyBaseUrl, ShouldRedirect)};
    mockHttpSessionAdapter->enqueueResponse(firstResponse);
    configcat::Response secondResponse = {200, createTestJson(ConfigFetcher::kEuOnlyBaseUrl, NoRedirect)};
    mockHttpSessionAdapter->enqueueResponse(secondResponse);
    auto fetchResponse = fetcher->fetchConfiguration();

    ASSERT_TRUE(fetchResponse.config != Config::empty);
    EXPECT_EQ(fetchResponse.config->preferences->url, ConfigFetcher::kEuOnlyBaseUrl);
    EXPECT_EQ(fetchResponse.config->preferences->redirect, NoRedirect);
    EXPECT_EQ(2, mockHttpSessionAdapter->requests.size());
    EXPECT_TRUE(starts_with(mockHttpSessionAdapter->requests[0], ConfigFetcher::kGlobalBaseUrl));
    EXPECT_TRUE(starts_with(mockHttpSessionAdapter->requests[1], ConfigFetcher::kEuOnlyBaseUrl));
}

TEST_F(ConfigFetcherTest, DataGovernance_ShouldRedirectToAnotherServerWhenForced) {
    SetUp();

    configcat::Response firstResponse = {200, createTestJson(ConfigFetcher::kEuOnlyBaseUrl, ForceRedirect)};
    mockHttpSessionAdapter->enqueueResponse(firstResponse);
    configcat::Response secondResponse = {200, createTestJson(ConfigFetcher::kEuOnlyBaseUrl, NoRedirect)};
    mockHttpSessionAdapter->enqueueResponse(secondResponse);
    auto fetchResponse = fetcher->fetchConfiguration();

    ASSERT_TRUE(fetchResponse.config != Config::empty);
    EXPECT_EQ(fetchResponse.config->preferences->url, ConfigFetcher::kEuOnlyBaseUrl);
    EXPECT_EQ(fetchResponse.config->preferences->redirect, NoRedirect);
    EXPECT_EQ(2, mockHttpSessionAdapter->requests.size());
    EXPECT_TRUE(starts_with(mockHttpSessionAdapter->requests[0], ConfigFetcher::kGlobalBaseUrl));
    EXPECT_TRUE(starts_with(mockHttpSessionAdapter->requests[1], ConfigFetcher::kEuOnlyBaseUrl));
}

TEST_F(ConfigFetcherTest, DataGovernance_ShouldBreakRedirectLoop) {
    SetUp();

    configcat::Response firstResponse = {200, createTestJson(ConfigFetcher::kEuOnlyBaseUrl, ShouldRedirect)};
    configcat::Response secondResponse = {200, createTestJson(ConfigFetcher::kGlobalBaseUrl, ShouldRedirect)};
    mockHttpSessionAdapter->enqueueResponse(firstResponse);
    mockHttpSessionAdapter->enqueueResponse(secondResponse);
    mockHttpSessionAdapter->enqueueResponse(firstResponse);
    auto fetchResponse = fetcher->fetchConfiguration();

    ASSERT_TRUE(fetchResponse.config != Config::empty);
    EXPECT_EQ(fetchResponse.config->preferences->url, ConfigFetcher::kEuOnlyBaseUrl);
    EXPECT_EQ(fetchResponse.config->preferences->redirect, ShouldRedirect);
    EXPECT_EQ(3, mockHttpSessionAdapter->requests.size());
    EXPECT_TRUE(starts_with(mockHttpSessionAdapter->requests[0], ConfigFetcher::kGlobalBaseUrl));
    EXPECT_TRUE(starts_with(mockHttpSessionAdapter->requests[1], ConfigFetcher::kEuOnlyBaseUrl));
    EXPECT_TRUE(starts_with(mockHttpSessionAdapter->requests[2], ConfigFetcher::kGlobalBaseUrl));
}

TEST_F(ConfigFetcherTest, DataGovernance_ShouldBreakRedirectLoopWhenForced) {
    SetUp();

    configcat::Response firstResponse = {200, createTestJson(ConfigFetcher::kEuOnlyBaseUrl, ForceRedirect)};
    configcat::Response secondResponse = {200, createTestJson(ConfigFetcher::kGlobalBaseUrl, ForceRedirect)};
    mockHttpSessionAdapter->enqueueResponse(firstResponse);
    mockHttpSessionAdapter->enqueueResponse(secondResponse);
    mockHttpSessionAdapter->enqueueResponse(firstResponse);
    auto fetchResponse = fetcher->fetchConfiguration();

    ASSERT_TRUE(fetchResponse.config != Config::empty);
    EXPECT_EQ(fetchResponse.config->preferences->url, ConfigFetcher::kEuOnlyBaseUrl);
    EXPECT_EQ(fetchResponse.config->preferences->redirect, ForceRedirect);
    EXPECT_EQ(3, mockHttpSessionAdapter->requests.size());
    EXPECT_TRUE(starts_with(mockHttpSessionAdapter->requests[0], ConfigFetcher::kGlobalBaseUrl));
    EXPECT_TRUE(starts_with(mockHttpSessionAdapter->requests[1], ConfigFetcher::kEuOnlyBaseUrl));
    EXPECT_TRUE(starts_with(mockHttpSessionAdapter->requests[2], ConfigFetcher::kGlobalBaseUrl));
}

TEST_F(ConfigFetcherTest, DataGovernance_ShouldRespectCustomUrl) {
    SetUp(kCustomCdnUrl);

    configcat::Response response = {200, createTestJson(ConfigFetcher::kGlobalBaseUrl, ShouldRedirect)};
    mockHttpSessionAdapter->enqueueResponse(response);
    auto fetchResponse = fetcher->fetchConfiguration();

    ASSERT_TRUE(fetchResponse.config != Config::empty);
    EXPECT_EQ(fetchResponse.config->preferences->url, ConfigFetcher::kGlobalBaseUrl);
    EXPECT_EQ(fetchResponse.config->preferences->redirect, ShouldRedirect);
    EXPECT_EQ(1, mockHttpSessionAdapter->requests.size());
    EXPECT_TRUE(starts_with(mockHttpSessionAdapter->requests[0], kCustomCdnUrl));
}

TEST_F(ConfigFetcherTest, DataGovernance_ShouldNotRespectCustomUrlWhenForced) {
    SetUp(kCustomCdnUrl);

    configcat::Response firstResponse = {200, createTestJson(ConfigFetcher::kGlobalBaseUrl, ForceRedirect)};
    configcat::Response secondResponse = {200, createTestJson(ConfigFetcher::kGlobalBaseUrl, NoRedirect)};
    mockHttpSessionAdapter->enqueueResponse(firstResponse);
    mockHttpSessionAdapter->enqueueResponse(secondResponse);
    auto fetchResponse = fetcher->fetchConfiguration();

    ASSERT_TRUE(fetchResponse.config != Config::empty);
    EXPECT_EQ(fetchResponse.config->preferences->url, ConfigFetcher::kGlobalBaseUrl);
    EXPECT_EQ(fetchResponse.config->preferences->redirect, NoRedirect);

    EXPECT_EQ(2, mockHttpSessionAdapter->requests.size());
    EXPECT_TRUE(starts_with(mockHttpSessionAdapter->requests[0], kCustomCdnUrl));
    EXPECT_TRUE(starts_with(mockHttpSessionAdapter->requests[1], ConfigFetcher::kGlobalBaseUrl));
}


