#pragma once

#include <limits>
#include <memory>

#include "configcat/config.h"

namespace configcat {

// extra brackets to avoid numeric_limits<double>::max()/min() not recognized error on windows
constexpr double kDistantFuture = (std::numeric_limits<double>::max)();
constexpr double kDistantPast = (std::numeric_limits<double>::min)();

struct ConfigEntry {
    static constexpr char kConfig[] = "config";
    static constexpr char kETag[] = "etag";
    static constexpr char kFetchTime[] = "fetch_time";
    static constexpr char kSerializationFormatVersion[] = "v2";

    static const std::shared_ptr<const ConfigEntry> empty;

    ConfigEntry(const std::shared_ptr<const Config>& config = Config::empty,
                const std::string& eTag = "",
                const std::string& configJsonString = "{}",
                double fetchTime = kDistantPast):
            config(config),
            eTag(eTag),
            configJsonString(configJsonString),
            fetchTime(fetchTime) {
    }
    ConfigEntry(const ConfigEntry&) = delete; // Disable copy

    static std::shared_ptr<const ConfigEntry> fromString(const std::string& text);
    std::string serialize() const;

    std::shared_ptr<const Config> config;
    std::string eTag;
    std::string configJsonString;
    double fetchTime;
};

} // namespace configcat
