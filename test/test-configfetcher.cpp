#include <gtest/gtest.h>
#include "configcat/configfetcher.h"
#include "configcat/configcatoptions.h"
#include "configcat/configjsoncache.h"
#include "mock.h"

using namespace configcat;
using namespace std;

TEST(ConfigFectherTest, CurlTest) {
    auto options = ConfigCatOptions();
    auto jsonCache = ConfigJsonCache("");
    auto fetcher = ConfigFetcher("", "", jsonCache, options);
}
