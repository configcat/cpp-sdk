#include <gtest/gtest.h>
#include "configcat/configfetcher.h"

using namespace configcat;

TEST(ConfigFectherTest, CurlTest) {
    auto fetcher = ConfigFetcher();

    EXPECT_EQ(200, fetcher.code);
    EXPECT_TRUE(fetcher.data.length() > 0);
}
