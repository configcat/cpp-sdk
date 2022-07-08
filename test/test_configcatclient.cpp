#include <gtest/gtest.h>
#include "configcat/configcatclient.hpp"
#include "configcat/consolelogger.hpp"

using namespace configcat;
using namespace std;


TEST(ConfigCatClientTest, GetValueTest) {
    ConsoleLogger logger;
    setLogger(&logger);
    setLogLevel(configcat::LOG_LEVEL_DEBUG);

    ConfigCatClient client;

    auto boolValue = client.getValue(string("bool"), false);
    auto boolValue2 = client.getValue("bool", false);
    const char* def = "default";
    auto stringValue = client.getValue(string("string"), def);
    char def2[100] = "def";
    auto stringValue2 = client.getValue(string("string"), def2);
    char* def3 = def2;
    auto stringValue3 = client.getValue(string("string"), def3);
    auto strValue = client.getValue(string("string"), "default");
    auto strValue2 = client.getValue("string", "default");
    auto intValue = client.getValue(string("int"), 0);
    auto otherValue = client.getValue(string("other"), 0.0);
}
