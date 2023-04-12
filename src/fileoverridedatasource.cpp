#include "configcat/fileoverridedatasource.h"
#include "configcat/configcatlogger.h"

using namespace std;

namespace configcat {

FileFlagOverrides::FileFlagOverrides(const std::string& filePath, OverrideBehaviour behaviour):
    filePath(filePath),
    behaviour(behaviour) {
}

std::shared_ptr<OverrideDataSource> FileFlagOverrides::createDataSource(std::shared_ptr<ConfigCatLogger> logger) {
    return make_shared<FileOverrideDataSource>(filePath, behaviour, logger);
}

FileOverrideDataSource::FileOverrideDataSource(const string& filePath, OverrideBehaviour behaviour, std::shared_ptr<ConfigCatLogger> logger):
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

const shared_ptr<unordered_map<string, Setting>> FileOverrideDataSource::getOverrides() {
    reloadFileContent();
    return overrides;
}

void FileOverrideDataSource::reloadFileContent() {
    try {
        auto lastWriteTime = std::filesystem::last_write_time(filePath);
        if (fileLastWriteTime != lastWriteTime) {
            fileLastWriteTime = lastWriteTime;
            auto config = Config::fromFile(filePath);
            overrides = config->entries;
        }
    } catch (filesystem::filesystem_error exception) {
        LOG_ERROR(1302) << "Failed to read the local config file '" << filePath << "'. " << exception.what();
    } catch (exception& exception) {
        LOG_ERROR(2302) << "Failed to decode JSON from the local config file '" << filePath << "'. " << exception.what();
    }
}

} // namespace configcat
