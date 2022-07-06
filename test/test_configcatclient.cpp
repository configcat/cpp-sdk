#include <gtest/gtest.h>
#include "configcat/configcatclient.hpp"

using namespace configcat;
using namespace std;

TEST(ConfigCatClientTest, BasicAssertions) {
    ConfigCatClient client;

    auto boolValue = client.getValue(string("bool"), false);
    auto boolValue2 = client.getValue("bool", false);
    const char* def = "default";
    auto stringValue = client.getValue(string("string"), def);
    auto strValue = client.getValue(string("string"), "default");
    auto strValue2 = client.getValue("string", "default");
    auto intValue = client.getValue(string("int"), 0);
    auto otherValue = client.getValue(string("other"), 0.0);
}
