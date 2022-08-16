#pragma once

#include <string>
#include <map>

namespace configcat {

struct Response {
    long status_code = 0;
    std::string text;
    std::map<std::string, std::string> header;
};

class HttpSessionAdapter {
public:
    virtual Response get(const std::string& url, const std::map<std::string, std::string>& header) = 0;
    virtual void close() = 0;
    virtual ~HttpSessionAdapter() = default;
};

} // namespace configcat
