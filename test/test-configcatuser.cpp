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

}
