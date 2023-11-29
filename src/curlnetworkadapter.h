#pragma once

#ifndef CONFIGCAT_EXTERNAL_NETWORK_ADAPTER_ENABLED

#include "configcat/httpsessionadapter.h"
#include <curl/curl.h>


namespace configcat {

class LibCurlResourceGuard;

class CurlNetworkAdapter : public HttpSessionAdapter {
public:
    CurlNetworkAdapter() = default;
    ~CurlNetworkAdapter();

    bool init(const HttpSessionObserver* httpSessionObserver, uint32_t connectTimeoutMs, uint32_t readTimeoutMs) override;
    Response get(const std::string& url,
                 const std::map<std::string, std::string>& header,
                 const std::map<std::string, std::string>& proxies,
                 const std::map<std::string, ProxyAuthentication>& proxyAuthentications) override;
    void close() override;

private:
    // CURL progress functions
    int ProgressFunction(curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow);
    friend int ProgressCallback(void* clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow);

    const HttpSessionObserver* httpSessionObserver = nullptr;
    std::shared_ptr<LibCurlResourceGuard> libCurlResourceGuard;
    CURL* curl = nullptr;
};

} // configcat

#endif // CONFIGCAT_EXTERNAL_NETWORK_ADAPTER_ENABLED
