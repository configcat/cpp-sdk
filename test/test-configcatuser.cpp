#include <gtest/gtest.h>
#include "configcat/configcatuser.h"
#include "configcat/log.h"

using namespace configcat;
using namespace std;

TEST(ConfigCatUserTest, UserAttributesCaseInsensitivity) {
    auto user = ConfigCatUser(
        "id",
        "email",
        "country", {
            {"custom", "test"}
        }
    );

    EXPECT_EQ("id", user.identifier);
    EXPECT_EQ("email", *user.getAttribute("Email"));
    EXPECT_EQ(nullptr, user.getAttribute("EMAIL"));
    EXPECT_EQ(nullptr, user.getAttribute("email"));
    EXPECT_EQ("country", *user.getAttribute("Country"));
    EXPECT_EQ(nullptr, user.getAttribute("COUNTRY"));
    EXPECT_EQ(nullptr, user.getAttribute("country"));
    EXPECT_EQ("test", *user.getAttribute("custom"));
    EXPECT_EQ(nullptr, user.getAttribute("not-existing"));
}
