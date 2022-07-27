#include <gtest/gtest.h>
#include "configcat/configfetcher.h"
#include "configcat/configcatoptions.h"
#include "configcat/configjsoncache.h"
#include "mock.h"

using namespace configcat;
using namespace std;

TEST(ConfigFectherTest, CurlTest) {
//    string testJson = R"({ "f": { "fakeKey": { "v": true, "p": [], "r": [] } } })";
//    auto fetcher = ConfigFetcher(make_shared<MockHttpSessionAdapter>(testJson));
    auto options = ConfigCatOptions();
    auto jsonCache = ConfigJsonCache("");
    auto fetcher = ConfigFetcher("", "", jsonCache, options);

    EXPECT_EQ(200, fetcher.code);
    EXPECT_TRUE(fetcher.data.length() > 0);
}
