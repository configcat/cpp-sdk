#include <gtest/gtest.h>
#include "configcatlogger.h"
#include "configcat/fileoverridedatasource.h"
#include "configcat/configcatclient.h"
#include <nlohmann/json.hpp>
#include "configcat/configcatuser.h"
#include "platform.h"
#include <fstream>
#include "test.h"
#include "mock.h"


using namespace configcat;
using namespace std;
using json = nlohmann::json;

class EvaluationLogTest : public ::testing::TestWithParam<string> {
public:
    static constexpr char kTestDataPath[] = "data/evaluation";
};

INSTANTIATE_TEST_SUITE_P(
    EvaluationLogTest,
    EvaluationLogTest,
    ::testing::Values(
        "simple_value",
        "1_targeting_rule",
        "2_targeting_rules",
        "options_based_on_user_id",
        "options_based_on_custom_attr",
        "options_after_targeting_rule",
        "options_within_targeting_rule",
        "and_rules",
        "segment",
        "prerequisite_flag",
        "comparators",
        "epoch_date_validation",
        "number_validation",
        "semver_validation",
        "list_truncation"
    ));

TEST_P(EvaluationLogTest, TestEvaluationLog) {
    string testSetName = GetParam();
    string directoryPath = RemoveFileName(__FILE__);

    string testSetDirectory = directoryPath + kTestDataPath + "/" + testSetName;
    string testSetPath = testSetDirectory + ".json";
    ASSERT_TRUE(filesystem::exists(testSetPath));
    ifstream file(testSetPath);
    json data = json::parse(file);
    auto sdkKey = data.value("sdkKey", "local-only");
    auto baseUrl = data.value("baseUrl", "");
    auto jsonOverride = data.value("jsonOverride", "");

    std::shared_ptr<TestLogger> testLogger = make_shared<TestLogger>();

    ConfigCatOptions options;
    options.pollingMode = PollingMode::manualPoll();
    options.logger = testLogger;
    if (!baseUrl.empty()) {
        options.baseUrl = baseUrl;
    }
    if (!jsonOverride.empty()) {
        options.flagOverrides = make_shared<FileFlagOverrides>(testSetDirectory + "/" + jsonOverride, LocalOnly);
    }

    auto client = ConfigCatClient::get(sdkKey, &options);
    client->forceRefresh();

    for (const auto& test : data["tests"]) {
        testLogger->clear();
        string key = test["key"];
        auto returnValue = test["returnValue"];
        auto defaultValue = test["defaultValue"];

        std::shared_ptr<ConfigCatUser> user;
        if (test.find("user") != test.end()) {
            json userJson = test["user"];
            string id;
            optional<string> email;
            optional<string> country;
            std::unordered_map<string, ConfigCatUser::AttributeValue> custom = {};
            for (const auto& [k, v] : userJson.items()) {
                if (k == "Identifier") {
                    id = v.get<string>();
                } else if (k == "Email") {
                    email = v.get<string>();
                } else if (k == "Country") {
                    country = v.get<string>();
                } else if (v.is_string()) {
                    custom[k] = v.get<string>();
                } else if (v.is_number()) {
                    custom[k] = v.get<double>();
                } else {
                    throw std::runtime_error("Custom user attribute value type is invalid.");
                }
            }

            user = make_shared<ConfigCatUser>(id, email, country, custom);
        }

        if (returnValue.is_boolean()) {
            auto value = client->getValue(key, defaultValue.get<bool>(), user);
            ASSERT_EQ(returnValue.get<bool>(), value);
        } else if (returnValue.is_string()) {
            auto value = client->getValue(key, defaultValue.get<string>(), user);
            ASSERT_EQ(returnValue.get<string>(), value);
        } else if (returnValue.is_number_integer()) {
            auto value = client->getValue(key, defaultValue.get<int32_t>(), user);
            ASSERT_EQ(returnValue.get<int32_t>(), value);
        } else if (returnValue.is_number()) {
            auto value = client->getValue(key, defaultValue.get<double>(), user);
            ASSERT_EQ(returnValue.get<double>(), value);
        } else {
            throw std::runtime_error("Return value type is invalid.");
        }

        auto expectedLogFile = test.value("expectedLog", "");
        auto expectedLogFilePath = testSetDirectory + "/" + expectedLogFile;

        // On Linux, the log file name is expected to be suffixed with "-linux" if it exists.
        if (contains(getPlatformName(), "linux")) {
            auto expectedLogFilePathLinux = expectedLogFilePath + ".linux";
            if (filesystem::exists(expectedLogFilePathLinux)) {
                expectedLogFilePath = expectedLogFilePathLinux;
            }
        }

        ASSERT_TRUE(filesystem::exists(expectedLogFilePath));
        std::ifstream logFile(expectedLogFilePath);
        std::stringstream buffer;
        buffer << logFile.rdbuf();
        auto expectedLog = buffer.str();

        ASSERT_EQ(expectedLog, testLogger->text);
    }

    ConfigCatClient::closeAll();
}
