#pragma once

#include <string>

namespace configcat {

struct Response {
    long status_code = 0;
    std::string text;
};

class HttpSessionAdapter {
public:
    virtual Response get(const std::string& url) = 0;
    virtual void close() = 0;
    virtual ~HttpSessionAdapter() = default;
};

} // namespace configcat
