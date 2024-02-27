#include <gtest/gtest.h>
#include "configcatlogger.h"
#include <nlohmann/json.hpp>

using namespace configcat;
using namespace std;
using json = nlohmann::json;

class LogTest : public ::testing::Test {
public:
    class TestLogger : public ILogger {
    public:
        TestLogger(): ILogger(LOG_LEVEL_INFO) {}
        void log(LogLevel level, const std::string& message, const std::optional<std::exception>& ex = std::nullopt) override {
            text += message + "\n";
        }
        std::string text;
    };

    std::shared_ptr<TestLogger> testLogger = make_shared<TestLogger>();
    shared_ptr<ConfigCatLogger> logger = make_shared<ConfigCatLogger>(testLogger, make_shared<Hooks>());
};

TEST_F(LogTest, LogUser) {
    auto user = ConfigCatUser(
        "id",
        "email",
        "country",
        {{"custom", "test"}}
    );
    LOG_INFO(0) << user;

    json userJson = json::parse(testLogger->text.substr(4));

    EXPECT_EQ("id", userJson["Identifier"]);
    EXPECT_EQ("email", userJson["Email"]);
    EXPECT_EQ("country", userJson["Country"]);
    EXPECT_EQ("test",  userJson["custom"]);
}

TEST_F(LogTest, LogIntVector) {
    std::vector<int> v = { 1, 2, 3 };
    LOG_INFO(5000) << v;
    EXPECT_EQ("[5000] [1, 2, 3]\n", testLogger->text);
}

TEST_F(LogTest, LogStringVector) {
    std::vector<string> v = { "a", "b", "c" };
    LOG_INFO(5000) << v;
    EXPECT_EQ("[5000] [a, b, c]\n", testLogger->text);
}
