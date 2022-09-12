#include <gtest/gtest.h>
#include "configcat/log.h"
#include "configcat/configcatuser.h"
#include <nlohmann/json.hpp>

using namespace configcat;
using namespace std;
using json = nlohmann::json;

class LogTest : public ::testing::Test {
public:
    class TestLogger : public ILogger {
    public:
        void log(LogLevel level, const std::string& message) override {
            text += message + "\n";
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

TEST_F(LogTest, LogUser) {
    auto user = ConfigCatUser(
        "id",
        "email",
        "country",
        {{"custom", "test"}}
    );
    LOG_INFO << user;

    json userJson = json::parse(testLogger.text);

    EXPECT_EQ("id", userJson["Identifier"]);
    EXPECT_EQ("email", userJson["Email"]);
    EXPECT_EQ("country", userJson["Country"]);
    EXPECT_EQ("test",  userJson["custom"]);
}

TEST_F(LogTest, LogIntVector) {
    std::vector<int> v = { 1, 2, 3 };
    LOG_INFO << v;
    EXPECT_EQ("[1, 2, 3]\n", testLogger.text);
}

TEST_F(LogTest, LogStringVector) {
    std::vector<string> v = { "a", "b", "c" };
    LOG_INFO << v;
    EXPECT_EQ("[a, b, c]\n", testLogger.text);
}
