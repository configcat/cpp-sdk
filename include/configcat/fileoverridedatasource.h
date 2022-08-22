#pragma once

#include <filesystem>

#include "overridedatasource.h"
#include "config.h"

namespace configcat {

class FileOverrideDataSource : public OverrideDataSource {
public:
    FileOverrideDataSource(const std::string& filePath);

    // Gets all the overrides defined in the given source.
    const std::shared_ptr<std::unordered_map<std::string, Setting>> getOverrides() override;

private:
    void reloadFileContent();

    const std::string filePath;
    std::filesystem::file_time_type fileLastWriteTime;
    std::shared_ptr<std::unordered_map<std::string, Setting>> overrides;
};

} // namespace configcat
