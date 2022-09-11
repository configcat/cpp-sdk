#include <gtest/gtest.h>
#include "configcat/log.h"

using namespace configcat;
using namespace std;

class LogTest : public ::testing::Test {
public:
    class TestLogger : public ILogger {
    public:
        void log(LogLevel level, const std::string& message) override {
            text += "["s + logLevelAsString(level) + "] "s + message + "\n";
        }
        std::string text;
    };

    ILogger* logger;
    TestLogger testLogger;

    LogTest() {
        logger = getLogger();
        setLogger(&testLogger);
    }

    ~LogTest() {
        setLogger(logger);
    }
};

TEST_F(LogTest, LogIntVector) {
    std::vector<int> v = { 1, 2, 3 };
    LOG_INFO << v;
    EXPECT_EQ("[Info] [1, 2, 3]\n", testLogger.text);
}

TEST_F(LogTest, LogStringVector) {
    std::vector<string> v = { "a", "b", "c" };
    LOG_INFO << v;
    EXPECT_EQ("[Info] [a, b, c]\n", testLogger.text);
}
