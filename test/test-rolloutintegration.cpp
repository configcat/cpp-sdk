#include <gtest/gtest.h>
#include <vector>
#include <fstream>
#include <list>
#include "configcat/configcatuser.h"
#include "configcat/configcatclient.h"
#include "configcatlogger.h"
#include "configcat/consolelogger.h"
#include "test.h"
#include "configcat/log.h"

using namespace std;
using namespace configcat;

using MatrixData = vector<vector<string>>;

class RolloutIntegrationTest : public ::testing::Test {
public:
    string directoryPath = RemoveFileName(__FILE__);
    shared_ptr<ConfigCatLogger> logger = make_shared<ConfigCatLogger>(make_shared<ConsoleLogger>(), make_shared<Hooks>());

    MatrixData loadMatrixData(const string& filePath) {
        MatrixData data;
        vector<string> row;
        string line;
        string word;

        fstream file(filePath);
        if (file.fail()) {
            LOG_ERROR(0) << "File open failed: " << filePath;
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

        auto client = ConfigCatClient::get(sdkKey);

        auto& header = matrixData[0];
        auto customKey = header[3];
        auto settingKeys = header;
        settingKeys.erase(settingKeys.begin(), settingKeys.begin() + 4);  // Drop first 4 items: "Identifier", "Email", "Country", "Custom1"
        list<string> errors;
        for (int i = 1; i < matrixData.size(); ++i) {
            auto& testObjects = matrixData[i];
            shared_ptr<ConfigCatUser> user;
            if (testObjects[0] != "##null##") {
                std::optional<std::string> email;
                std::optional<std::string> country;
                string identifier = testObjects[0];
                if (!testObjects[1].empty() && testObjects[1] != "##null##") {
                    email = testObjects[1];
                }
                if (!testObjects[2].empty() && testObjects[2] != "##null##") {
                    country = testObjects[2];
                }

                unordered_map<string, ConfigCatUser::AttributeValue> custom;
                if (!testObjects[3].empty() && testObjects[3] != "##null##") {
                    custom[customKey] = testObjects[3];
                }

                user = make_shared<ConfigCatUser>(identifier, email, country, custom);
            }

            int j = 0;
            for (auto& settingKey : settingKeys) {
                string expected = testObjects[j + 4];
                if (isValueKind) {
                    auto value = client->getValue(settingKey, user);
                    
                    auto success = value.has_value();
                    if (success) {
                        if (holds_alternative<bool>(*value)) {
                            success = get<bool>(*value) ? expected == "True" : expected == "False";
                        } else if (holds_alternative<string>(*value)) {
                            success = get<string>(*value) == expected;
                        } else if (holds_alternative<int32_t>(*value)) {
                            success = get<int32_t>(*value) == stoi(expected);
                        } else {
                            success = get<double>(*value) == stod(expected);
                        }
                    }

                    if (!success) {
                        errors.push_back(string_format("Index: [%d:%d] Identifier: %s, Key: %s. UV: %s Expected: %s, Result: %s",
                                         i, j,
                                         testObjects[0].c_str(),
                                         settingKey.c_str(),
                                         testObjects[3].c_str(),
                                         expected.c_str(),
                                         value ? value->toString().c_str() : "##null##"));
                    }
                } else {
                    auto details = client->getValueDetails(settingKey, user);
                    auto variationId = details.variationId;
                    if (variationId != expected) {
                        errors.push_back(
                                string_format("Index: [%d:%d] Identifier: %s, Key: %s. Expected: %s, Result: %s",
                                              i, j,
                                              testObjects[0].c_str(),
                                              settingKey.c_str(),
                                              expected.c_str(),
                                              variationId.value_or("").c_str()));
                    }
                }
                ++j;
            }
        }

        if (!errors.empty()) {
            for (auto& error : errors) {
                LOG_ERROR(0) << error;
            }

            FAIL() << "Errors found: " << errors.size();
        }
    }
};

// *** Config V1 ***

TEST_F(RolloutIntegrationTest, RolloutMatrixText_v1) {
    // https://app.configcat.com/08d5a03c-feb7-af1e-a1fa-40b3329f8bed/08d62463-86ec-8fde-f5b5-1c5c426fc830/244cf8b0-f604-11e8-b543-f23c917f9d8d
    testRolloutMatrix(directoryPath + "data/testmatrix.csv", "PKDVCLf-Hq-h-kCzMp-L7Q/psuH7BGHoUmdONrzzUOY7A", true);
}

TEST_F(RolloutIntegrationTest, RolloutMatrixSemantic_v1) {
    // https://app.configcat.com/08d5a03c-feb7-af1e-a1fa-40b3329f8bed/08d745f1-f315-7daf-d163-5541d3786e6f/244cf8b0-f604-11e8-b543-f23c917f9d8d
    testRolloutMatrix(directoryPath + "data/testmatrix_semantic.csv", "PKDVCLf-Hq-h-kCzMp-L7Q/BAr3KgLTP0ObzKnBTo5nhA", true);
}

TEST_F(RolloutIntegrationTest, RolloutMatrixNumber_v1) {
    // https://app.configcat.com/08d5a03c-feb7-af1e-a1fa-40b3329f8bed/08d747f0-5986-c2ef-eef3-ec778e32e10a/244cf8b0-f604-11e8-b543-f23c917f9d8d
    testRolloutMatrix(directoryPath + "data/testmatrix_number.csv", "PKDVCLf-Hq-h-kCzMp-L7Q/uGyK3q9_ckmdxRyI7vjwCw", true);
}

TEST_F(RolloutIntegrationTest, RolloutMatrixSemantic2_v1) {
    // https://app.configcat.com/08d5a03c-feb7-af1e-a1fa-40b3329f8bed/08d77fa1-a796-85f9-df0c-57c448eb9934/244cf8b0-f604-11e8-b543-f23c917f9d8d
    testRolloutMatrix(directoryPath + "data/testmatrix_semantic_2.csv", "PKDVCLf-Hq-h-kCzMp-L7Q/q6jMCFIp-EmuAfnmZhPY7w", true);
}

TEST_F(RolloutIntegrationTest, RolloutMatrixSensitive_v1) {
    // https://app.configcat.com/08d5a03c-feb7-af1e-a1fa-40b3329f8bed/08d7b724-9285-f4a7-9fcd-00f64f1e83d5/244cf8b0-f604-11e8-b543-f23c917f9d8d
    testRolloutMatrix(directoryPath + "data/testmatrix_sensitive.csv", "PKDVCLf-Hq-h-kCzMp-L7Q/qX3TP2dTj06ZpCCT1h_SPA", true);
}

TEST_F(RolloutIntegrationTest, RolloutMatrixSegmentsOld_v1) {
    // https://app.configcat.com/08d5a03c-feb7-af1e-a1fa-40b3329f8bed/08d9f207-6883-43e5-868c-cbf677af3fe6/244cf8b0-f604-11e8-b543-f23c917f9d8d
    testRolloutMatrix(directoryPath + "data/testmatrix_segments_old.csv", "PKDVCLf-Hq-h-kCzMp-L7Q/LcYz135LE0qbcacz2mgXnA", true);
}

TEST_F(RolloutIntegrationTest, RolloutMatrixVariationId_v1) {
    // https://app.configcat.com/08d5a03c-feb7-af1e-a1fa-40b3329f8bed/08d774b9-3d05-0027-d5f4-3e76c3dba752/244cf8b0-f604-11e8-b543-f23c917f9d8d
    testRolloutMatrix(directoryPath + "data/testmatrix_variationId.csv", "PKDVCLf-Hq-h-kCzMp-L7Q/nQ5qkhRAUEa6beEyyrVLBA", false);
}

// *** Config V2 ***

TEST_F(RolloutIntegrationTest, RolloutMatrixText) {
    // https://app.configcat.com/v2/e7a75611-4256-49a5-9320-ce158755e3ba/08d5a03c-feb7-af1e-a1fa-40b3329f8bed/08dbc4dc-1927-4d6b-8fb9-b1472564e2d3/244cf8b0-f604-11e8-b543-f23c917f9d8d
    testRolloutMatrix(directoryPath + "data/testmatrix.csv", "configcat-sdk-1/PKDVCLf-Hq-h-kCzMp-L7Q/AG6C1ngVb0CvM07un6JisQ", true);
}

TEST_F(RolloutIntegrationTest, RolloutMatrixSemantic) {
    // https://app.configcat.com/v2/e7a75611-4256-49a5-9320-ce158755e3ba/08d5a03c-feb7-af1e-a1fa-40b3329f8bed/08dbc4dc-1927-4d6b-8fb9-b1472564e2d3/244cf8b0-f604-11e8-b543-f23c917f9d8d
    testRolloutMatrix(directoryPath + "data/testmatrix_semantic.csv", "configcat-sdk-1/PKDVCLf-Hq-h-kCzMp-L7Q/iV8vH2MBakKxkFZylxHmTg", true);
}

TEST_F(RolloutIntegrationTest, RolloutMatrixNumber) {
    // https://app.configcat.com/v2/e7a75611-4256-49a5-9320-ce158755e3ba/08d5a03c-feb7-af1e-a1fa-40b3329f8bed/08dbc4dc-0fa3-48d0-8de8-9de55b67fb8b/244cf8b0-f604-11e8-b543-f23c917f9d8d
    testRolloutMatrix(directoryPath + "data/testmatrix_number.csv", "configcat-sdk-1/PKDVCLf-Hq-h-kCzMp-L7Q/FCWN-k1dV0iBf8QZrDgjdw", true);
}

TEST_F(RolloutIntegrationTest, RolloutMatrixSemantic2) {
    // https://app.configcat.com/v2/e7a75611-4256-49a5-9320-ce158755e3ba/08d5a03c-feb7-af1e-a1fa-40b3329f8bed/08dbc4dc-2b2b-451e-8359-abdef494c2a2/244cf8b0-f604-11e8-b543-f23c917f9d8d
    testRolloutMatrix(directoryPath + "data/testmatrix_semantic_2.csv", "configcat-sdk-1/PKDVCLf-Hq-h-kCzMp-L7Q/U8nt3zEhDEO5S2ulubCopA", true);
}

TEST_F(RolloutIntegrationTest, RolloutMatrixSensitive) {
    // https://app.configcat.com/v2/e7a75611-4256-49a5-9320-ce158755e3ba/08d5a03c-feb7-af1e-a1fa-40b3329f8bed/08dbc4dc-2d62-4e1b-884b-6aa237b34764/244cf8b0-f604-11e8-b543-f23c917f9d8d
    testRolloutMatrix(directoryPath + "data/testmatrix_sensitive.csv", "configcat-sdk-1/PKDVCLf-Hq-h-kCzMp-L7Q/-0YmVOUNgEGKkgRF-rU65g", true);
}

TEST_F(RolloutIntegrationTest, RolloutMatrixSegmentsOld) {
    // https://app.configcat.com/v2/e7a75611-4256-49a5-9320-ce158755e3ba/08d5a03c-feb7-af1e-a1fa-40b3329f8bed/08dbd6ca-a85f-4ed0-888a-2da18def92b5/244cf8b0-f604-11e8-b543-f23c917f9d8d
    testRolloutMatrix(directoryPath + "data/testmatrix_segments_old.csv", "configcat-sdk-1/PKDVCLf-Hq-h-kCzMp-L7Q/y_ZB7o-Xb0Swxth-ZlMSeA", true);
}

TEST_F(RolloutIntegrationTest, RolloutMatrixVariationId) {
    // https://app.configcat.com/v2/e7a75611-4256-49a5-9320-ce158755e3ba/08d5a03c-feb7-af1e-a1fa-40b3329f8bed/08dbc4dc-30c6-4969-8e4c-03f6a8764199/244cf8b0-f604-11e8-b543-f23c917f9d8d
    testRolloutMatrix(directoryPath + "data/testmatrix_variationId.csv", "configcat-sdk-1/PKDVCLf-Hq-h-kCzMp-L7Q/spQnkRTIPEWVivZkWM84lQ", false);
}

TEST_F(RolloutIntegrationTest, RolloutMatrixAndOr) {
    // https://app.configcat.com/v2/e7a75611-4256-49a5-9320-ce158755e3ba/08dbc325-7f69-4fd4-8af4-cf9f24ec8ac9/08dbc325-9d5e-4988-891c-fd4a45790bd1/08dbc325-9ebd-4587-8171-88f76a3004cb
    testRolloutMatrix(directoryPath + "data/testmatrix_and_or.csv", "configcat-sdk-1/JcPbCGl_1E-K9M-fJOyKyQ/ByMO9yZNn02kXcm72lnY1A", true);
}

TEST_F(RolloutIntegrationTest, RolloutMatrixComparatorsV6) {
    // https://app.configcat.com/v2/e7a75611-4256-49a5-9320-ce158755e3ba/08dbc325-7f69-4fd4-8af4-cf9f24ec8ac9/08dbc325-9a6b-4947-84e2-91529248278a/08dbc325-9ebd-4587-8171-88f76a3004cb
    testRolloutMatrix(directoryPath + "data/testmatrix_comparators_v6.csv", "configcat-sdk-1/JcPbCGl_1E-K9M-fJOyKyQ/OfQqcTjfFUGBwMKqtyEOrQ", true);
}

TEST_F(RolloutIntegrationTest, RolloutMatrixPrerequisiteFlag) {
    // https://app.configcat.com/v2/e7a75611-4256-49a5-9320-ce158755e3ba/08dbc325-7f69-4fd4-8af4-cf9f24ec8ac9/08dbc325-9b74-45cb-86d0-4d61c25af1aa/08dbc325-9ebd-4587-8171-88f76a3004cb
    testRolloutMatrix(directoryPath + "data/testmatrix_prerequisite_flag.csv", "configcat-sdk-1/JcPbCGl_1E-K9M-fJOyKyQ/JoGwdqJZQ0K2xDy7LnbyOg", true);
}
TEST_F(RolloutIntegrationTest, RolloutMatrixSegments) {
    // https://app.configcat.com/v2/e7a75611-4256-49a5-9320-ce158755e3ba/08dbc325-7f69-4fd4-8af4-cf9f24ec8ac9/08dbc325-9b74-45cb-86d0-4d61c25af1aa/08dbc325-9ebd-4587-8171-88f76a3004cb
    testRolloutMatrix(directoryPath + "data/testmatrix_segments.csv", "configcat-sdk-1/JcPbCGl_1E-K9M-fJOyKyQ/h99HYXWWNE2bH8eWyLAVMA", true);
}
TEST_F(RolloutIntegrationTest, RolloutMatrixUnicode) {
    // https://app.configcat.com/v2/e7a75611-4256-49a5-9320-ce158755e3ba/08dbc325-7f69-4fd4-8af4-cf9f24ec8ac9/08dbc325-9b74-45cb-86d0-4d61c25af1aa/08dbc325-9ebd-4587-8171-88f76a3004cb
    testRolloutMatrix(directoryPath + "data/testmatrix_unicode.csv", "configcat-sdk-1/JcPbCGl_1E-K9M-fJOyKyQ/Da6w8dBbmUeMUBhh0iEeQQ", true);
}
