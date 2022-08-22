#include "configcat/fileoverridedatasource.h"

using namespace std;

namespace configcat {

FileOverrideDataSource::FileOverrideDataSource(const string& filePath):
    overrides(make_shared<unordered_map<string, Setting>>()),
    filePath(filePath) {
}

const shared_ptr<unordered_map<string, Setting>> FileOverrideDataSource::getOverrides() {
    reloadFileContent();
    return overrides;
}

void FileOverrideDataSource::reloadFileContent() {
    auto lastWriteTime = std::filesystem::last_write_time(filePath);
    if (fileLastWriteTime != lastWriteTime) {
        fileLastWriteTime = lastWriteTime;
        auto config = Config::fromFile(filePath);
        overrides = config->entries;
    }
}

} // namespace configcat
