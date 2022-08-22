#include <string>

#if defined(_WIN32)
const char kPathSeparator = '\\';
#else
const char kPathSeparator = '/';
#endif

// Returns the directory path with the filename removed.
// Example: FilePath("path/to/file").RemoveFileName() returns "path/to/".
inline std::string RemoveFileName(std::string path) {
    const size_t lastSeparatorIndex = path.rfind(kPathSeparator);
    if (lastSeparatorIndex) {
        return path.substr(0, lastSeparatorIndex + 1);
    }
    return path;
}
