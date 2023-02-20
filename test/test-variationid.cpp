#include <gtest/gtest.h>
#include "mock.h"
#include "configcat/configcatclient.h"
#include "configcat/config.h"
#include "utils.h"


using namespace configcat;
using namespace std;

class VariationIdTest : public ::testing::Test {
public:
    static constexpr char kTestSdkKey[] = "TestSdkKey";
    static constexpr char kTestJson[] = R"(
                                        {"f":{
                                            "key1":{
                                                "v":true,
                                                "i":"fakeId1",
                                                "p":[
                                                    {
                                                        "v":true,
                                                        "p":50,
                                                        "i":"percentageId1"
                                                    },
                                                    {
                                                        "v":false,
                                                        "p":50,
                                                        "i":"percentageId2"
                                                    }
                                                ],
                                                "r":[
                                                    {
                                                        "a":"Email",
                                                        "t":2,
                                                        "c":"@configcat.com",
                                                        "v":true,
                                                        "i":"rolloutId1"
                                                    },
                                                    {
                                                        "a":"Email",
                                                        "t":2,
                                                        "c":"@test.com",
                                                        "v":false,
                                                        "i":"rolloutId2"
                                                    }
                                                ]
                                            },
                                            "key2":{
                                                "v":false,
                                                "i":"fakeId2",
                                                "p":[],
                                                "r":[]
                                            }
                                        }}
                                        )";

    ConfigCatClient* client = nullptr;
    shared_ptr<MockHttpSessionAdapter> mockHttpSessionAdapter = make_shared<MockHttpSessionAdapter>();

    void SetUp() override {
        ConfigCatOptions options;
        options.pollingMode = PollingMode::manualPoll();
        options.httpSessionAdapter = mockHttpSessionAdapter;

        client = ConfigCatClient::get(kTestSdkKey, &options);
    }

    void TearDown() override {
        ConfigCatClient::closeAll();
    }
};

TEST_F(VariationIdTest, GetVariationId) {
    configcat::Response response = {200, kTestJson};
    mockHttpSessionAdapter->enqueueResponse(response);
    client->forceRefresh();
    auto variationId = client->getVariationId("key1", "");

    EXPECT_EQ("fakeId1", variationId);
}

TEST_F(VariationIdTest, GetVariationIdNotFound) {
    configcat::Response response = {200, kTestJson};
    mockHttpSessionAdapter->enqueueResponse(response);
    client->forceRefresh();
    auto variationId = client->getVariationId("nonexisting", "default");

    EXPECT_EQ("default", variationId);
}

TEST_F(VariationIdTest, GetVarationIdInvalidJson) {
    configcat::Response response = {200, "{"};
    mockHttpSessionAdapter->enqueueResponse(response);
    client->forceRefresh();
    auto variationId = client->getVariationId("key1", "default");

    EXPECT_EQ("default", variationId);
}

TEST_F(VariationIdTest, GetAllVariationIds) {
    configcat::Response response = {200, kTestJson};
    mockHttpSessionAdapter->enqueueResponse(response);
    client->forceRefresh();
    auto variationIds = client->getAllVariationIds();

    EXPECT_EQ(2, variationIds.size());
    EXPECT_TRUE(std::find(variationIds.begin(), variationIds.end(), "fakeId1") != variationIds.end());
    EXPECT_TRUE(std::find(variationIds.begin(), variationIds.end(), "fakeId2") != variationIds.end());
}

TEST_F(VariationIdTest, GetAllVariationIdsEmpty) {
    configcat::Response response = {200, "{}"};
    mockHttpSessionAdapter->enqueueResponse(response);
    client->forceRefresh();
    auto variationIds = client->getAllVariationIds();

    EXPECT_EQ(0, variationIds.size());
}

TEST_F(VariationIdTest, GetKeyAndValue) {
    configcat::Response response = {200, kTestJson};
    mockHttpSessionAdapter->enqueueResponse(response);
    client->forceRefresh();

    auto result = client->getKeyAndValue("fakeId2");
    EXPECT_TRUE(result != nullptr);
    EXPECT_EQ("key2", result->key);
    EXPECT_FALSE(std::get<bool>(result->value));

    result = client->getKeyAndValue("percentageId2");
    EXPECT_TRUE(result != nullptr);
    EXPECT_EQ("key1", result->key);
    EXPECT_FALSE(std::get<bool>(result->value));

    result = client->getKeyAndValue("rolloutId2");
    EXPECT_TRUE(result != nullptr);
    EXPECT_EQ("key1", result->key);
    EXPECT_FALSE(std::get<bool>(result->value));
}

TEST_F(VariationIdTest, GetKeyAndValueNotFound) {
    configcat::Response response = {200, "{}"};
    mockHttpSessionAdapter->enqueueResponse(response);
    client->forceRefresh();

    auto result = client->getKeyAndValue("nonexisting");
    EXPECT_TRUE(result == nullptr);
}
