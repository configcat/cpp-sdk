#include <gtest/gtest.h>
#include "configcat/configfetcher.h"
#include "mock.h"

using namespace configcat;
using namespace std;

TEST(ConfigFectherTest, CurlTest) {
    string testJson = R"({ "f": { "fakeKey": { "v": true, "p": [], "r": [] } } })";
//    auto fetcher = ConfigFetcher(make_shared<MockHttpSessionAdapter>(testJson));
    auto fetcher = ConfigFetcher({});

    EXPECT_EQ(200, fetcher.code);
    EXPECT_TRUE(fetcher.data.length() > 0);
}
