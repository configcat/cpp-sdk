#pragma once

#include <filesystem>

#include "overridedatasource.h"
#include "config.h"

namespace configcat {

class FileFlagOverrides : public FlagOverrides {
public:
    FileFlagOverrides(const std::string& filePath, OverrideBehaviour behaviour);
    std::shared_ptr<OverrideDataSource> createDataSource(const std::shared_ptr<ConfigCatLogger>& logger) override;

    inline OverrideBehaviour getBehavior() override { return this->behaviour; }

private:
    const std::string filePath;
    OverrideBehaviour behaviour;
};


class FileOverrideDataSource : public OverrideDataSource {
public:
    FileOverrideDataSource(const std::string& filePath, OverrideBehaviour behaviour, const std::shared_ptr<ConfigCatLogger>& logger);

    // Gets all the overrides defined in the given source.
    std::shared_ptr<Settings> getOverrides() override;

private:
    void reloadFileContent();

    const std::string filePath;
    std::shared_ptr<ConfigCatLogger> logger;
    std::filesystem::file_time_type fileLastWriteTime;
    std::shared_ptr<Settings> overrides;
};

} // namespace configcat
