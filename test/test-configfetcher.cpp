#include <gtest/gtest.h>
#include "configfetcher.h"
#include "configcat/configcatoptions.h"
#include "configcatlogger.h"
#include "configcat/consolelogger.h"
#include "mock.h"
#include "utils.h"

using namespace configcat;
using namespace std;

class ConfigFetcherTest : public ::testing::Test {
public:
    static constexpr char kTestSdkKey[] = "TestSdkKey";
    static constexpr char kCustomCdnUrl[] = "https://custom-cdn.configcat.com";
    static constexpr char kTestJson[] = R"({"f":{"fakeKey":{"t":1,"v":{"s":"fakeValue"}}}})";
    unique_ptr<ConfigFetcher> fetcher;
    shared_ptr<MockHttpSessionAdapter> mockHttpSessionAdapter = make_shared<MockHttpSessionAdapter>();
    shared_ptr<ConfigCatLogger> logger = make_shared<ConfigCatLogger>(make_shared<ConsoleLogger>(), make_shared<Hooks>());

    void SetUp(const std::string& baseUrl = "", const std::string& sdkKey = kTestSdkKey) {
        ConfigCatOptions options;
        options.pollingMode = PollingMode::manualPoll();
        options.httpSessionAdapter = mockHttpSessionAdapter;
        options.baseUrl = baseUrl;

        fetcher = make_unique<ConfigFetcher>(sdkKey, logger, "m", options);
    }

    void TearDown() override {
    }

    string createTestJson(const string& url, RedirectMode redirectMode) {
        return string_format(R"({"p":{"u":"%s","r":%d}})", url.c_str(), redirectMode);
    }
};

TEST_F(ConfigFetcherTest, DataGovernance_ShouldStayOnGivenUrl) {
    SetUp();

    configcat::Response response = {200, createTestJson(ConfigFetcher::kGlobalBaseUrl, RedirectMode::No)};
    mockHttpSessionAdapter->enqueueResponse(response);
    auto fetchResponse = fetcher->fetchConfiguration();

    ASSERT_TRUE(fetchResponse.entry != ConfigEntry::empty);
    ASSERT_TRUE(fetchResponse.entry->config != Config::empty);
    auto& config = fetchResponse.entry->config;
    EXPECT_EQ(config->preferences->baseUrl, ConfigFetcher::kGlobalBaseUrl);
    EXPECT_EQ(config->preferences->redirectMode, RedirectMode::No);
    EXPECT_EQ(1, mockHttpSessionAdapter->requests.size());
    EXPECT_TRUE(starts_with(mockHttpSessionAdapter->requests[0].url, ConfigFetcher::kGlobalBaseUrl));
}

TEST_F(ConfigFetcherTest, DataGovernance_ShouldStayOnSameUrl) {
    SetUp();

    configcat::Response response = {200, createTestJson(ConfigFetcher::kGlobalBaseUrl, RedirectMode::Should)};
    mockHttpSessionAdapter->enqueueResponse(response);
    auto fetchResponse = fetcher->fetchConfiguration();

    ASSERT_TRUE(fetchResponse.entry != ConfigEntry::empty);
    ASSERT_TRUE(fetchResponse.entry->config != Config::empty);
    auto& config = fetchResponse.entry->config;
    EXPECT_EQ(config->preferences->baseUrl, ConfigFetcher::kGlobalBaseUrl);
    EXPECT_EQ(config->preferences->redirectMode, RedirectMode::Should);
    EXPECT_EQ(1, mockHttpSessionAdapter->requests.size());
    EXPECT_TRUE(starts_with(mockHttpSessionAdapter->requests[0].url, ConfigFetcher::kGlobalBaseUrl));
}

TEST_F(ConfigFetcherTest, DataGovernance_ShouldStayOnSameUrlEvenWithForce) {
    SetUp();

    configcat::Response response = {200, createTestJson(ConfigFetcher::kGlobalBaseUrl, RedirectMode::Force)};
    mockHttpSessionAdapter->enqueueResponse(response);
    auto fetchResponse = fetcher->fetchConfiguration();

    ASSERT_TRUE(fetchResponse.entry != ConfigEntry::empty);
    ASSERT_TRUE(fetchResponse.entry->config != Config::empty);
    auto& config = fetchResponse.entry->config;
    EXPECT_EQ(config->preferences->baseUrl, ConfigFetcher::kGlobalBaseUrl);
    EXPECT_EQ(config->preferences->redirectMode, RedirectMode::Force);
    EXPECT_EQ(1, mockHttpSessionAdapter->requests.size());
    EXPECT_TRUE(starts_with(mockHttpSessionAdapter->requests[0].url, ConfigFetcher::kGlobalBaseUrl));
}

TEST_F(ConfigFetcherTest, DataGovernance_ShouldRedirectToAnotherServer) {
    SetUp();

    configcat::Response firstResponse = {200, createTestJson(ConfigFetcher::kEuOnlyBaseUrl, RedirectMode::Should)};
    mockHttpSessionAdapter->enqueueResponse(firstResponse);
    configcat::Response secondResponse = {200, createTestJson(ConfigFetcher::kEuOnlyBaseUrl, RedirectMode::No)};
    mockHttpSessionAdapter->enqueueResponse(secondResponse);
    auto fetchResponse = fetcher->fetchConfiguration();

    ASSERT_TRUE(fetchResponse.entry != ConfigEntry::empty);
    ASSERT_TRUE(fetchResponse.entry->config != Config::empty);
    auto& config = fetchResponse.entry->config;
    EXPECT_EQ(config->preferences->baseUrl, ConfigFetcher::kEuOnlyBaseUrl);
    EXPECT_EQ(config->preferences->redirectMode, RedirectMode::No);
    EXPECT_EQ(2, mockHttpSessionAdapter->requests.size());
    EXPECT_TRUE(starts_with(mockHttpSessionAdapter->requests[0].url, ConfigFetcher::kGlobalBaseUrl));
    EXPECT_TRUE(starts_with(mockHttpSessionAdapter->requests[1].url, ConfigFetcher::kEuOnlyBaseUrl));
}

TEST_F(ConfigFetcherTest, DataGovernance_ShouldRedirectToAnotherServerWhenForced) {
    SetUp();

    configcat::Response firstResponse = {200, createTestJson(ConfigFetcher::kEuOnlyBaseUrl, RedirectMode::Force)};
    mockHttpSessionAdapter->enqueueResponse(firstResponse);
    configcat::Response secondResponse = {200, createTestJson(ConfigFetcher::kEuOnlyBaseUrl, RedirectMode::No)};
    mockHttpSessionAdapter->enqueueResponse(secondResponse);
    auto fetchResponse = fetcher->fetchConfiguration();

    ASSERT_TRUE(fetchResponse.entry != ConfigEntry::empty);
    ASSERT_TRUE(fetchResponse.entry->config != Config::empty);
    auto& config = fetchResponse.entry->config;
    EXPECT_EQ(config->preferences->baseUrl, ConfigFetcher::kEuOnlyBaseUrl);
    EXPECT_EQ(config->preferences->redirectMode, RedirectMode::No);
    EXPECT_EQ(2, mockHttpSessionAdapter->requests.size());
    EXPECT_TRUE(starts_with(mockHttpSessionAdapter->requests[0].url, ConfigFetcher::kGlobalBaseUrl));
    EXPECT_TRUE(starts_with(mockHttpSessionAdapter->requests[1].url, ConfigFetcher::kEuOnlyBaseUrl));
}

TEST_F(ConfigFetcherTest, DataGovernance_ShouldBreakRedirectLoop) {
    SetUp();

    configcat::Response firstResponse = {200, createTestJson(ConfigFetcher::kEuOnlyBaseUrl, RedirectMode::Should)};
    configcat::Response secondResponse = {200, createTestJson(ConfigFetcher::kGlobalBaseUrl, RedirectMode::Should)};
    mockHttpSessionAdapter->enqueueResponse(firstResponse);
    mockHttpSessionAdapter->enqueueResponse(secondResponse);
    mockHttpSessionAdapter->enqueueResponse(firstResponse);
    auto fetchResponse = fetcher->fetchConfiguration();

    ASSERT_TRUE(fetchResponse.entry != ConfigEntry::empty);
    ASSERT_TRUE(fetchResponse.entry->config != Config::empty);
    auto& config = fetchResponse.entry->config;
    EXPECT_EQ(config->preferences->baseUrl, ConfigFetcher::kEuOnlyBaseUrl);
    EXPECT_EQ(config->preferences->redirectMode, RedirectMode::Should);
    EXPECT_EQ(3, mockHttpSessionAdapter->requests.size());
    EXPECT_TRUE(starts_with(mockHttpSessionAdapter->requests[0].url, ConfigFetcher::kGlobalBaseUrl));
    EXPECT_TRUE(starts_with(mockHttpSessionAdapter->requests[1].url, ConfigFetcher::kEuOnlyBaseUrl));
    EXPECT_TRUE(starts_with(mockHttpSessionAdapter->requests[2].url, ConfigFetcher::kGlobalBaseUrl));
}

TEST_F(ConfigFetcherTest, DataGovernance_ShouldBreakRedirectLoopWhenForced) {
    SetUp();

    configcat::Response firstResponse = {200, createTestJson(ConfigFetcher::kEuOnlyBaseUrl, RedirectMode::Force)};
    configcat::Response secondResponse = {200, createTestJson(ConfigFetcher::kGlobalBaseUrl, RedirectMode::Force)};
    mockHttpSessionAdapter->enqueueResponse(firstResponse);
    mockHttpSessionAdapter->enqueueResponse(secondResponse);
    mockHttpSessionAdapter->enqueueResponse(firstResponse);
    auto fetchResponse = fetcher->fetchConfiguration();

    ASSERT_TRUE(fetchResponse.entry != ConfigEntry::empty);
    ASSERT_TRUE(fetchResponse.entry->config != Config::empty);
    auto& config = fetchResponse.entry->config;
    EXPECT_EQ(config->preferences->baseUrl, ConfigFetcher::kEuOnlyBaseUrl);
    EXPECT_EQ(config->preferences->redirectMode, RedirectMode::Force);
    EXPECT_EQ(3, mockHttpSessionAdapter->requests.size());
    EXPECT_TRUE(starts_with(mockHttpSessionAdapter->requests[0].url, ConfigFetcher::kGlobalBaseUrl));
    EXPECT_TRUE(starts_with(mockHttpSessionAdapter->requests[1].url, ConfigFetcher::kEuOnlyBaseUrl));
    EXPECT_TRUE(starts_with(mockHttpSessionAdapter->requests[2].url, ConfigFetcher::kGlobalBaseUrl));
}

TEST_F(ConfigFetcherTest, DataGovernance_ShouldRespectCustomUrl) {
    SetUp(kCustomCdnUrl);

    configcat::Response response = {200, createTestJson(ConfigFetcher::kGlobalBaseUrl, RedirectMode::Should)};
    mockHttpSessionAdapter->enqueueResponse(response);
    auto fetchResponse = fetcher->fetchConfiguration();

    ASSERT_TRUE(fetchResponse.entry != ConfigEntry::empty);
    ASSERT_TRUE(fetchResponse.entry->config != Config::empty);
    auto& config = fetchResponse.entry->config;
    EXPECT_EQ(config->preferences->baseUrl, ConfigFetcher::kGlobalBaseUrl);
    EXPECT_EQ(config->preferences->redirectMode, RedirectMode::Should);
    EXPECT_EQ(1, mockHttpSessionAdapter->requests.size());
    EXPECT_TRUE(starts_with(mockHttpSessionAdapter->requests[0].url, kCustomCdnUrl));
}

TEST_F(ConfigFetcherTest, DataGovernance_ShouldNotRespectCustomUrlWhenForced) {
    SetUp(kCustomCdnUrl);

    configcat::Response firstResponse = {200, createTestJson(ConfigFetcher::kGlobalBaseUrl, RedirectMode::Force)};
    configcat::Response secondResponse = {200, createTestJson(ConfigFetcher::kGlobalBaseUrl, RedirectMode::No)};
    mockHttpSessionAdapter->enqueueResponse(firstResponse);
    mockHttpSessionAdapter->enqueueResponse(secondResponse);
    auto fetchResponse = fetcher->fetchConfiguration();

    ASSERT_TRUE(fetchResponse.entry != ConfigEntry::empty);
    ASSERT_TRUE(fetchResponse.entry->config != Config::empty);
    auto& config = fetchResponse.entry->config;
    EXPECT_EQ(config->preferences->baseUrl, ConfigFetcher::kGlobalBaseUrl);
    EXPECT_EQ(config->preferences->redirectMode, RedirectMode::No);

    EXPECT_EQ(2, mockHttpSessionAdapter->requests.size());
    EXPECT_TRUE(starts_with(mockHttpSessionAdapter->requests[0].url, kCustomCdnUrl));
    EXPECT_TRUE(starts_with(mockHttpSessionAdapter->requests[1].url, ConfigFetcher::kGlobalBaseUrl));
}

TEST_F(ConfigFetcherTest, Fetcher_SimpleFetchSuccess) {
    SetUp();

    configcat::Response response = {200, kTestJson};
    mockHttpSessionAdapter->enqueueResponse(response);

    auto eTag = "";
    auto fetchResponse = fetcher->fetchConfiguration(eTag);

    EXPECT_TRUE(fetchResponse.isFetched());
    EXPECT_TRUE(fetchResponse.entry != nullptr && fetchResponse.entry != ConfigEntry::empty);
    auto entries = *fetchResponse.entry->config->settings;
    EXPECT_EQ("fakeValue", get<string>(entries["fakeKey"].value));
}

TEST_F(ConfigFetcherTest, Fetcher_SimpleFetchNotModified) {
    SetUp();

    configcat::Response response = {304, ""};
    mockHttpSessionAdapter->enqueueResponse(response);

    auto eTag = "";
    auto fetchResponse = fetcher->fetchConfiguration(eTag);

    EXPECT_TRUE(fetchResponse.notModified());
    EXPECT_EQ(ConfigEntry::empty, fetchResponse.entry);
}

TEST_F(ConfigFetcherTest, Fetcher_SimpleFetchFailed404) {
    SetUp();

    configcat::Response response = {404, ""};
    mockHttpSessionAdapter->enqueueResponse(response);

    auto eTag = "";
    auto fetchResponse = fetcher->fetchConfiguration(eTag);

    EXPECT_TRUE(fetchResponse.isFailed());
    EXPECT_FALSE(fetchResponse.isTransientError);
    EXPECT_EQ(ConfigEntry::empty, fetchResponse.entry);
}

TEST_F(ConfigFetcherTest, Fetcher_SimpleFetchFailed403) {
    SetUp();

    configcat::Response response = {403, ""};
    mockHttpSessionAdapter->enqueueResponse(response);

    auto eTag = "";
    auto fetchResponse = fetcher->fetchConfiguration(eTag);

    EXPECT_TRUE(fetchResponse.isFailed());
    EXPECT_FALSE(fetchResponse.isTransientError);
    EXPECT_EQ(ConfigEntry::empty, fetchResponse.entry);
}

TEST_F(ConfigFetcherTest, Fetcher_FetchNotModifiedEtag) {
    SetUp();

    auto eTag = "test";
    configcat::Response firstResponse = {200, kTestJson, {{"ETag", eTag}}};
    mockHttpSessionAdapter->enqueueResponse(firstResponse);
    configcat::Response secondResponse = {304, ""};
    mockHttpSessionAdapter->enqueueResponse(secondResponse);

    auto fetchResponse = fetcher->fetchConfiguration("");

    EXPECT_TRUE(fetchResponse.isFetched());
    EXPECT_TRUE(fetchResponse.entry != nullptr && fetchResponse.entry != ConfigEntry::empty);
    EXPECT_EQ(eTag, fetchResponse.entry->eTag);
    auto entries = *fetchResponse.entry->config->settings;
    EXPECT_EQ("fakeValue", get<string>(entries["fakeKey"].value));

    fetchResponse = fetcher->fetchConfiguration(eTag);
    EXPECT_TRUE(fetchResponse.notModified());
    EXPECT_EQ(ConfigEntry::empty, fetchResponse.entry);
    EXPECT_EQ(eTag, mockHttpSessionAdapter->requests.back().header["If-None-Match"]);
}

TEST_F(ConfigFetcherTest, Fethcer_ServerSideEtag) {
    ConfigCatOptions options;
    options.pollingMode = PollingMode::manualPoll();

    auto fetcher = make_unique<ConfigFetcher>("PKDVCLf-Hq-h-kCzMp-L7Q/HhOWfwVtZ0mb30i9wi17GQ", logger, "m", options);

    auto fetchResponse = fetcher->fetchConfiguration();
    auto eTag = fetchResponse.entry->eTag;
    EXPECT_TRUE(!eTag.empty());
    EXPECT_TRUE(fetchResponse.isFetched());
    EXPECT_FALSE(fetchResponse.notModified());

    fetchResponse = fetcher->fetchConfiguration(eTag);
    EXPECT_FALSE(fetchResponse.isFetched());
    EXPECT_TRUE(fetchResponse.notModified());

    fetchResponse = fetcher->fetchConfiguration("");
    EXPECT_TRUE(fetchResponse.isFetched());
    EXPECT_FALSE(fetchResponse.notModified());
}
