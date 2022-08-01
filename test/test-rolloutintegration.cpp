#include <gtest/gtest.h>
#include <vector>
#include <fstream>
#include <list>
#include "configcat/configcatuser.h"
#include "configcat/configcatclient.h"
#include "configcat/utils.h"
#include "configcat/log.h"


#include <iostream>
#include <string>
#include <filesystem>

using namespace std;
using namespace configcat;

using MatrixData = vector<vector<string>>;

#if defined(_WIN32)
const char kPathSeparator = '\\';
#else
const char kPathSeparator = '/';
#endif

// Returns the directory path with the filename removed.
// Example: FilePath("path/to/file").RemoveFileName() returns "path/to/".
static string RemoveFileName(string path) {
    const size_t lastSeparatorIndex = path.rfind(kPathSeparator);
    if (lastSeparatorIndex) {
        return path.substr(0, lastSeparatorIndex + 1);
    }
    return path;
}

class RolloutIntegrationTest : public ::testing::Test {
public:
    string directoryPath = RemoveFileName(__FILE__);
    LogLevel logLevel;

    RolloutIntegrationTest() {
        // Minimal logging during rollout integration tests
        logLevel = getLogLevel();
        setLogLevel(LOG_LEVEL_WARNING);
    }

    ~RolloutIntegrationTest() {
        setLogLevel(logLevel);
    }

    MatrixData loadMatrixData(const string& filePath) {
        MatrixData data;
        vector<string> row;
        string line;
        string word;

        fstream file(filePath);
        if (file.fail()) {
            LOG_ERROR << "File open failed: " << filePath;
            return {};
        }

        while (getline(file, line)) {
            row.clear();
            stringstream stream(line);
            while (getline(stream, word, ';')) {
                row.push_back(word);
            }
            data.push_back(row);
        }

        return data;
    }

    void testRolloutMatrix(const string& filePath, const string& sdkKey, bool isValueKind) {
        auto matrixData = loadMatrixData(filePath);
        if (matrixData.empty()) {
            FAIL() << "Matrix data is empty.";
        }

        ConfigCatOptions options;
        options.mode = PollingMode::manualPoll();
        auto client = ConfigCatClient::get(sdkKey, options);
        client->forceRefresh(); // TODO use autopoll here

        auto& header = matrixData[0];
        auto customKey = header[3];
        auto settingKeys = header;
        settingKeys.erase(settingKeys.begin(), settingKeys.begin() + 4);  // Drop first 4 items: "Identifier", "Email", "Country", "Custom1"
        list<string> errors;
        for (int i = 1; i < matrixData.size(); ++i) {
            auto& testObjects = matrixData[i];
            unique_ptr<ConfigCatUser> user;
            if (testObjects[0] != "##null##") {
                string email;
                string country;
                string identifier = testObjects[0];
                if (!testObjects[1].empty() && testObjects[1] != "##null##") {
                    email = testObjects[1];
                }
                if (!testObjects[2].empty() && testObjects[2] != "##null##") {
                    country = testObjects[2];
                }

                unordered_map<string, string> custom;
                if (!testObjects[3].empty() && testObjects[3] != "##null##") {
                    custom[customKey] = testObjects[3];
                }

                user = make_unique<ConfigCatUser>(identifier, email, country, custom);
            }

            int j = 0;
            for (auto& settingKey : settingKeys) {
                string expected = str_tolower(testObjects[j + 4]);
                if (isValueKind) {
                    auto value = client->getValue(settingKey, user.get());
                    if (!value || str_tolower(valueToString(*value)) != expected) {
                        errors.push_back(string_format("Index: [%d:%d] Identifier: %s, Key: %s. UV: %s Expected: %s, Result: %s",
                                         i, j,
                                         testObjects[0].c_str(),
                                         settingKey.c_str(),
                                         testObjects[3].c_str(),
                                         expected.c_str(),
                                         value ? valueToString(*value).c_str() : "##null##"));
                    }
                } else {
                    auto variationId = client->getVariationId(settingKey, "", user.get());
                    if (variationId != expected) {
                        errors.push_back(
                                string_format("Index: [%d:%d] Identifier: %s, Key: %s. Expected: %s, Result: %s",
                                              i, j,
                                              testObjects[0].c_str(),
                                              settingKey.c_str(),
                                              expected.c_str(),
                                              variationId.c_str()));
                    }
                }
                ++j;
            }
        }

        if (!errors.empty()) {
            for (auto& error : errors) {
                LOG_ERROR << error;
            }

            FAIL() << "Errors found: " << errors.size();
        }
    }
};

TEST_F(RolloutIntegrationTest, RolloutMatrixText) {
    testRolloutMatrix(directoryPath + "data/testmatrix.csv", "PKDVCLf-Hq-h-kCzMp-L7Q/psuH7BGHoUmdONrzzUOY7A", true);
}

//TEST_F(RolloutIntegrationTest, RolloutMatrixSemantic) {
//    testRolloutMatrix(directoryPath + "data/testmatrix_semantic.csv", "PKDVCLf-Hq-h-kCzMp-L7Q/BAr3KgLTP0ObzKnBTo5nhA", true);
//}
//
//TEST_F(RolloutIntegrationTest, RolloutMatrixSemantic2) {
//    testRolloutMatrix(directoryPath + "data/testmatrix_semantic2.csv", "PKDVCLf-Hq-h-kCzMp-L7Q/q6jMCFIp-EmuAfnmZhPY7w", true);
//}

TEST_F(RolloutIntegrationTest, RolloutMatrixNumber) {
    testRolloutMatrix(directoryPath + "data/testmatrix_number.csv", "PKDVCLf-Hq-h-kCzMp-L7Q/uGyK3q9_ckmdxRyI7vjwCw", true);
}

TEST_F(RolloutIntegrationTest, RolloutMatrixSensitive) {
    testRolloutMatrix(directoryPath + "data/testmatrix_sensitive.csv", "PKDVCLf-Hq-h-kCzMp-L7Q/qX3TP2dTj06ZpCCT1h_SPA", true);
}

TEST_F(RolloutIntegrationTest, RolloutMatrixVariationId) {
    testRolloutMatrix(directoryPath + "data/testmatrix_variationId.csv", "PKDVCLf-Hq-h-kCzMp-L7Q/nQ5qkhRAUEa6beEyyrVLBA", false);
}
