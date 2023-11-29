#pragma once

#include <string>
#include <map>
#include "proxyauthentication.h"

namespace configcat {

struct Response {
    long statusCode = 0;
    std::string text;
    std::map<std::string, std::string> header;

    bool operationTimedOut = false;
    std::string error;
};

class HttpSessionAdapterListener {
public:
    virtual bool isClosed() const = 0;
    virtual ~HttpSessionAdapterListener() = default;
};

class HttpSessionAdapter {
public:
    virtual bool init(const HttpSessionAdapterListener* sessionAdapterListener, uint32_t connectTimeoutMs, uint32_t readTimeoutMs) = 0;
    virtual Response get(const std::string& url,
                         const std::map<std::string, std::string>& header,
                         const std::map<std::string, std::string>& proxies,
                         const std::map<std::string, ProxyAuthentication>& proxyAuthentications) = 0;
    virtual void close() = 0;
    virtual ~HttpSessionAdapter() = default;
};

} // namespace configcat
