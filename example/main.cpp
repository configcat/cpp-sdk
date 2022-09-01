#include <configcat/configcatclient.h>
#include <configcat/configcatuser.h>
#include <iostream>

using namespace std;
using namespace configcat;

int main(int /*argc*/, char** /*argv*/) {
    auto client = ConfigCatClient::get("PKDVCLf-Hq-h-kCzMp-L7Q/HhOWfwVtZ0mb30i9wi17GQ");
    auto user = ConfigCatUser(
        "user-id",
        "configcat@example.com",
        "country", {
            {"version", "1.0.0"}
        }
    );

    bool value = client->getValue("isPOCFeatureEnabled", false, &user);
    cout << "isPOCFeatureEnabled value from ConfigCat: " << (value == true ? "true" : "false");

    ConfigCatClient::close();

    return 0;
}
