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

TEST_F(LogTest, LogStringVector1) {
    std::vector<string> v = { "a", "b", "c" };
    LOG_INFO(5000) << v;
    EXPECT_EQ("[5000] ['a', 'b', 'c']\n", testLogger->text);
}
