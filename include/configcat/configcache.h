#pragma once

#include <string>

namespace configcat {

// A cache API used to make custom cache implementations.
class ConfigCache {
public:
    /**
     * Child classes has to implement this method, the [ConfigCatClient] is
     * using it to get the actual value from the cache.
     *
     * [key] is the key of the cache entry.
     */
    virtual const std::string& read(const std::string& key) = 0;

    /**
     * Child classes has to implement this method, the [ConfigCatClient] is
     * using it to set the actual cached value.
     *
     * [key] is the key of the cache entry.
     * [value] is the new value to cache.
     */
    virtual void write(const std::string& key, const std::string& value) = 0;

    virtual ~ConfigCache() = default;
};

} // namespace configcat
