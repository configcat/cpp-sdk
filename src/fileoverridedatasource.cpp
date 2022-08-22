#include "configcat/fileoverridedatasource.h"
#include "configcat/log.h"

using namespace std;

namespace configcat {

FileOverrideDataSource::FileOverrideDataSource(const string& filePath):
    overrides(make_shared<unordered_map<string, Setting>>()),
    filePath(filePath) {
    if (!filesystem::exists(filePath)) {
        LOG_ERROR <<  "The file " << filePath << " does not exists.";
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
        LOG_ERROR << exception.what();
    }
}

} // namespace configcat
