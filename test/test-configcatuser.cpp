#include <gtest/gtest.h>
#include "configcat/configcatuser.h"
#include "configcat/log.h"
#include <nlohmann/json.hpp>

using namespace configcat;
using namespace std;
using json = nlohmann::json;

TEST(ConfigCatUserTest, UserAttributesCaseInsensitivity) {
    auto user = ConfigCatUser(
        "id",
        "email",
        "country", {
            {"custom", "test"}
        }
    );

    EXPECT_EQ("id", user.getIdentifier());
    EXPECT_EQ("email", get<string>(*user.getAttribute("Email")));
    EXPECT_EQ(nullptr, user.getAttribute("EMAIL"));
    EXPECT_EQ(nullptr, user.getAttribute("email"));
    EXPECT_EQ("country", get<string>(*user.getAttribute("Country")));
    EXPECT_EQ(nullptr, user.getAttribute("COUNTRY"));
    EXPECT_EQ(nullptr, user.getAttribute("country"));
    EXPECT_EQ("test", get<string>(*user.getAttribute("custom")));
    EXPECT_EQ(nullptr, user.getAttribute("not-existing"));
}

TEST(ConfigCatUserTest, ToJson) {
    auto user = ConfigCatUser(
        "id",
        "email",
        "country", {
            {"string", "test"},
            {"datetime", make_datetime(2023, 9, 19, 11, 1, 35, 999)},
            {"int", 42},
            {"double", 3.14}
        }
    );

    json userJson = json::parse(user.toJson());

    EXPECT_EQ("id", userJson["Identifier"]);
    EXPECT_EQ("email", userJson["Email"]);
    EXPECT_EQ("country", userJson["Country"]);
    EXPECT_EQ("test", userJson["string"]);
    EXPECT_EQ(42, userJson["int"]);
    EXPECT_EQ(3.14, userJson["double"]);
    EXPECT_EQ("2023-09-19T11:01:35.999Z"s, userJson["datetime"]);
}

