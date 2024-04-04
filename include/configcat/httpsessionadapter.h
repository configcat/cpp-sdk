#pragma once

#include <string>
#include <map>
#include "proxyauthentication.h"

namespace configcat {

enum class ResponseErrorCode : int {
    OK = 0,
    TimedOut = 1,
    RequestCancelled = 2,
    InternalError = 3
};

struct Response {
    long statusCode = 0;
    std::string text;
    std::map<std::string, std::string> header;

    ResponseErrorCode errorCode = ResponseErrorCode::OK;
    std::string error;
};

class HttpSessionAdapter {
public:
    virtual bool init(uint32_t connectTimeoutMs, uint32_t readTimeoutMs) = 0;
    virtual Response get(const std::string& url,
                         const std::map<std::string, std::string>& header,
                         const std::map<std::string, std::string>& proxies,
                         const std::map<std::string, ProxyAuthentication>& proxyAuthentications) = 0;
    virtual void close() = 0;
    virtual ~HttpSessionAdapter() = default;
};

} // namespace configcat
