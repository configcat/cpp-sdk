#include <gtest/gtest.h>
#include "mock.h"

using namespace configcat;
using namespace std;

class LogTest : public ::testing::Test {
public:
    std::shared_ptr<TestLogger> testLogger = make_shared<TestLogger>();
    shared_ptr<ConfigCatLogger> logger = make_shared<ConfigCatLogger>(testLogger, make_shared<Hooks>());
};

TEST_F(LogTest, LogStringVector1) {
    std::vector<string> v = { "a", "b", "c" };
    LOG_INFO(5000) << v;
    EXPECT_EQ("INFO [5000] ['a', 'b', 'c']\n", testLogger->text);
}
