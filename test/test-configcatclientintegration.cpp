#include <gtest/gtest.h>
#include "mock.h"
#include "configcat/configcatclient.h"

using namespace configcat;
using namespace std;

TEST(ConfigCatClientIntegrationTest, RequestTimeout) {
    ConfigCatOptions options;
    options.pollingMode = PollingMode::autoPoll(120);
    options.connectTimeoutMs = 1; // milliseconds
    options.readTimeoutMs = 1; // milliseconds
    auto client = ConfigCatClient::get("PKDVCLf-Hq-h-kCzMp-L7Q/psuH7BGHoUmdONrzzUOY7A", &options);
    auto startTime = chrono::steady_clock::now();

    auto value = client->getValue("stringDefaultCat", "");
    EXPECT_EQ("", value);

    auto endTime = chrono::steady_clock::now();
    auto elapsedTimeInSeconds = chrono::duration<double>(endTime - startTime).count();
    EXPECT_TRUE(elapsedTimeInSeconds < 1);
}

TEST(ConfigCatClientIntegrationTest, DISABLED_ProxyTest) {
    /*
     * How to run a local Squid proxy server:
     * https://hub.docker.com/r/ubuntu/squid
     * docker run -d --name squid-container -e TZ=UTC -p 3128:3128 ubuntu/squid:5.2-22.04_beta
     *
     * How to test the proxy server:
     * curl --proxy localhost:3128 https://cdn-global.configcat.com/configuration-files/PKDVCLf-Hq-h-kCzMp-L7Q/psuH7BGHoUmdONrzzUOY7A/config_v6.json
     *
     */
    ConfigCatOptions options;
    options.pollingMode = PollingMode::manualPoll();
    options.proxies = {{"https", "localhost:3128"}};
    auto client = ConfigCatClient::get("PKDVCLf-Hq-h-kCzMp-L7Q/psuH7BGHoUmdONrzzUOY7A", &options);
    client->forceRefresh();

    auto value = client->getValue("stringDefaultCat", "");
    EXPECT_EQ("Cat", value);
}
