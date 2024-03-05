#include "configcat/fileoverridedatasource.h"
#include "configcatlogger.h"

using namespace std;

namespace configcat {

FileFlagOverrides::FileFlagOverrides(const std::string& filePath, OverrideBehaviour behaviour):
    filePath(filePath),
    behaviour(behaviour) {
}

std::shared_ptr<OverrideDataSource> FileFlagOverrides::createDataSource(const std::shared_ptr<ConfigCatLogger>& logger) {
    return make_shared<FileOverrideDataSource>(filePath, behaviour, logger);
}

FileOverrideDataSource::FileOverrideDataSource(const string& filePath, OverrideBehaviour behaviour, const std::shared_ptr<ConfigCatLogger>& logger):
    OverrideDataSource(behaviour),
    overrides(make_shared<unordered_map<string, Setting>>()),
    filePath(filePath),
    logger(logger) {
    if (!filesystem::exists(filePath)) {
        LOG_ERROR(1300) <<
            "Cannot find the local config file '" << filePath << "'. "
            "This is a path that your application provided to the ConfigCat SDK by passing it to the constructor of the `FileFlagOverrides` class. "
            "Read more: https://configcat.com/docs/sdk-reference/cpp/#json-file";
    }
}

shared_ptr<unordered_map<string, Setting>> FileOverrideDataSource::getOverrides() {
    reloadFileContent();
    return overrides;
}

void FileOverrideDataSource::reloadFileContent() {
    try {
        auto lastWriteTime = std::filesystem::last_write_time(filePath);
        if (fileLastWriteTime != lastWriteTime) {
            fileLastWriteTime = lastWriteTime;
            auto config = Config::fromFile(filePath);
            overrides = config->getSettingsOrEmpty();
        }
    } catch (const filesystem::filesystem_error&) {
        LogEntry logEntry(logger, configcat::LOG_LEVEL_ERROR, 1302, current_exception());
        logEntry << "Failed to read the local config file '" << filePath << "'.";
    } catch (...) {
        LogEntry logEntry(logger, configcat::LOG_LEVEL_ERROR, 2302, current_exception());
        logEntry << "Failed to decode JSON from the local config file '" << filePath << "'.";
    }
}

} // namespace configcat
