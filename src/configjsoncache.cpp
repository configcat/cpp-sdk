#include "configcat/configjsoncache.h"
#include "configcat/configfetcher.h"
#include "configcat/configcatcache.h"

using namespace std;

namespace configcat {

ConfigJsonCache::ConfigJsonCache(string sdkKey, shared_ptr<ConfigCatCache> cache):
    cache(cache),
    inMemoryConfig(make_shared<Config>()){
    cacheKey = string("cpp_") + ConfigFetcher::kConfigJsonName + "_" + sdkKey;
}

shared_ptr<Config> ConfigJsonCache::readFromJson(const string& json, const string& eTag) {
    if (json.empty()) {
        return Config::empty;
    }

    auto config = Config::fromJson(json, eTag);
    return config;
}

shared_ptr<Config> ConfigJsonCache::readCache() {
    string fromCache = cache ? cache->read(cacheKey) : "";
    if (fromCache.empty() || fromCache == inMemoryConfig->jsonString) {
        return inMemoryConfig;
    }

    inMemoryConfig = Config::fromJson(fromCache);;
    return inMemoryConfig;
}

void ConfigJsonCache::writeCache(shared_ptr<Config> config) {
    inMemoryConfig = config;
    if (cache) {
        cache->write(cacheKey, inMemoryConfig->jsonString);
    }
}

} // namespace configcat
