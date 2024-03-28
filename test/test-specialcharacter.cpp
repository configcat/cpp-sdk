#include <gtest/gtest.h>
#include "configcat/configcatuser.h"
#include "configcat/configcatclient.h"


using namespace std;
using namespace configcat;

using MatrixData = vector<vector<string>>;

class SpecialCharacterTest : public ::testing::Test {
public:
    shared_ptr<ConfigCatClient> client = nullptr;
    void SetUp() override {
        client = ConfigCatClient::get("configcat-sdk-1/PKDVCLf-Hq-h-kCzMp-L7Q/u28_1qNyZ0Wz-ldYHIU7-g");
    }

    void TearDown() override {
        ConfigCatClient::closeAll();
    }
};

TEST_F(SpecialCharacterTest, SpecialCharactersWorksCleartext) {
    string actual = client->getValue("specialCharacters", "NOT_CAT", make_shared<ConfigCatUser>("äöüÄÖÜçéèñışğâ¢™✓😀"));
    EXPECT_EQ(actual, "äöüÄÖÜçéèñışğâ¢™✓😀");
}

TEST_F(SpecialCharacterTest, SpecialCharactersWorksHashed) {
    string actual = client->getValue("specialCharactersHashed", "NOT_CAT", make_shared<ConfigCatUser>("äöüÄÖÜçéèñışğâ¢™✓😀"));
    EXPECT_EQ(actual, "äöüÄÖÜçéèñışğâ¢™✓😀");
}
