#include <configcat/configcat.h>
#include <configcat/consolelogger.h>
#include <iostream>

using namespace std;
using namespace configcat;

int main(int /*argc*/, char** /*argv*/) {
    // Info level logging helps to inspect the feature flag evaluation process.
    // Use the default warning level to avoid too detailed logging in your application.
    auto logger = make_shared<ConsoleLogger>(LOG_LEVEL_INFO);

    // Initialize the ConfigCatClient with an SDK Key.
    ConfigCatOptions options;
    options.logger = logger;
    auto client = ConfigCatClient::get("PKDVCLf-Hq-h-kCzMp-L7Q/HhOWfwVtZ0mb30i9wi17GQ", &options);

    // Creating a user object to identify your user (optional).
    auto user = ConfigCatUser(
        "user-id",
        "configcat@example.com",
        "country",
        { { "version", "1.0.0" } }
    );

    bool value = client->getValue("isPOCFeatureEnabled", false, &user);
    cout << "isPOCFeatureEnabled value from ConfigCat: " << (value == true ? "true" : "false");

    ConfigCatClient::closeAll();

    return 0;
}
