#include <gtest/gtest.h>
#include <vector>
#include <fstream>
#include <list>
#include "configcat/configcatuser.h"
#include "configcat/configcatclient.h"
#include "configcat/utils.h"


#include <iostream>
#include <string>
#include <filesystem>
#include <unistd.h>

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
static string RemoveFileName(string path){
    const size_t lastSeparatorIndex = path.rfind(kPathSeparator);
    if (lastSeparatorIndex) {
        return path.substr(0, lastSeparatorIndex + 1);
    }
    return path;
}

class RolloutIntegrationTest : public ::testing::Test {
public:
    string directoryPath = RemoveFileName(__FILE__);

    MatrixData loadMatrixData(const string& filePath) {
        MatrixData data;
        vector<string> row;
        string line;
        string word;

        fstream file(filePath);
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
        ConfigCatOptions options;
        options.mode = PollingMode::manualPoll();
        auto client = ConfigCatClient::get(sdkKey, options);
        client->forceRefresh(); // TODO use autopoll here

        auto& header = matrixData[0];
        auto customKey = header[3];
        auto& settingKeys = header;
        settingKeys.resize(settingKeys.size() - 4); // Drop last items
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
                if (isValueKind) {
                    // TODO: variation_id
                    auto value = client->getValue(settingKey, user.get());
                    string expected = str_tolower(testObjects[j + 4]);
                    if (!value || str_tolower(valueToString(*value)) != expected) {
                        errors.push_back(string_format("Identifier: %s, Key: %s. UV: %s Expected: %s, Result: %s",
                                         testObjects[0].c_str(),
                                         settingKey.c_str(),
                                         testObjects[3].c_str(),
                                         expected.c_str(),
                                         value ? valueToString(*value).c_str() : "##null##"));
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
