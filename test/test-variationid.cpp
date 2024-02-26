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
    static constexpr char kTestJson[] = R"({
      "f": {
        "key1": {
          "t": 0,
          "r": [
            {
              "c": [
                {
                  "u": {
                    "a": "Email",
                    "c": 2,
                    "l": [
                      "@configcat.com"
                    ]
                  }
                }
              ],
              "s": {
                "v": {
                  "b": true
                },
                "i": "rolloutId1"
              }
            },
            {
              "c": [
                {
                  "u": {
                    "a": "Email",
                    "c": 2,
                    "l": [
                      "@test.com"
                    ]
                  }
                }
              ],
              "s": {
                "v": {
                  "b": false
                },
                "i": "rolloutId2"
              }
            }
          ],
          "p": [
            {
              "p": 50,
              "v": {
                "b": true
              },
              "i": "percentageId1"
            },
            {
              "p": 50,
              "v": {
                "b": false
              },
              "i": "percentageId2"
            }
          ],
          "v": {
            "b": true
          },
          "i": "fakeId1"
        },
        "key2": {
          "t": 0,
          "v": {
            "b": false
          },
          "i": "fakeId2"
        }
      }
    })";

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
    auto details = client->getValueDetails("key1", "");

    EXPECT_EQ("fakeId1", details.variationId);
}

TEST_F(VariationIdTest, GetVariationIdNotFound) {
    configcat::Response response = {200, kTestJson};
    mockHttpSessionAdapter->enqueueResponse(response);
    client->forceRefresh();
    auto details = client->getValueDetails("nonexisting", "default");
    EXPECT_EQ("", details.variationId);
}

TEST_F(VariationIdTest, GetVarationIdInvalidJson) {
    configcat::Response response = {200, "{"};
    mockHttpSessionAdapter->enqueueResponse(response);
    client->forceRefresh();
    auto details = client->getValueDetails("key1", "");

    EXPECT_EQ("", details.variationId);
}

TEST_F(VariationIdTest, GetAllVariationIds) {
    configcat::Response response = {200, kTestJson};
    mockHttpSessionAdapter->enqueueResponse(response);
    client->forceRefresh();
    auto allDetails = client->getAllValueDetails();

    EXPECT_EQ(2, allDetails.size());
    EXPECT_TRUE(std::find_if(allDetails.begin(), allDetails.end(), [] (const EvaluationDetails<Value>& details) {
        return details.variationId == "fakeId1"; }) != allDetails.end());
    EXPECT_TRUE(std::find_if(allDetails.begin(), allDetails.end(), [] (const EvaluationDetails<Value>& details) {
        return details.variationId == "fakeId2"; }) != allDetails.end());
}

TEST_F(VariationIdTest, GetAllValueDetailsEmpty) {
    configcat::Response response = {200, "{}"};
    mockHttpSessionAdapter->enqueueResponse(response);
    client->forceRefresh();
    auto allDetails = client->getAllValueDetails();

    EXPECT_EQ(0, allDetails.size());
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
