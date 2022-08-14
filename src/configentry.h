#pragma once

#include <memory>
#include <chrono>
#include "configcat/config.h"

namespace configcat {

struct ConfigEntry {
    static inline std::shared_ptr<ConfigEntry> empty = std::make_shared<ConfigEntry>();

    ConfigEntry(const std::string& jsonString = "{}",
                std::shared_ptr<Config> config = Config::empty,
                const std::string& eTag = "",
                std::chrono::time_point<std::chrono::steady_clock> fetchTime = {}):
        jsonString(jsonString),
        config(config),
        eTag(eTag),
        fetchTime(fetchTime) {
    }
    ConfigEntry(const ConfigEntry&) = delete; // Disable copy

    std::string jsonString;
    std::shared_ptr<Config> config;
    std::string eTag;
    std::chrono::time_point<std::chrono::steady_clock> fetchTime;
};

} // namespace configcat
