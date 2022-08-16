#include <gtest/gtest.h>
#include "mock.h"
#include "configcat/configcatclient.h"
#include "utils.h"

using namespace configcat;
using namespace std;

TEST(ConfigCatClientIntegrationTest, RequestTimeout) {
    ConfigCatOptions options;
    options.mode = PollingMode::autoPoll(120);
    options.connectTimeout = 1; // milliseconds
    options.readTimeout = 1; // milliseconds
    auto client = ConfigCatClient::get("PKDVCLf-Hq-h-kCzMp-L7Q/psuH7BGHoUmdONrzzUOY7A", options);
    auto startTime = chrono::steady_clock::now();

    auto value = client->getValue("stringDefaultCat", "");
    EXPECT_EQ("", value);

    auto endTime = chrono::steady_clock::now();
    auto elapsedTimeInSeconds = chrono::duration<double>(endTime - startTime).count();
    EXPECT_TRUE(elapsedTimeInSeconds < 1);
}
