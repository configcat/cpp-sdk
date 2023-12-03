#pragma once

#ifndef CONFIGCAT_EXTERNAL_NETWORK_ADAPTER_ENABLED

#include "configcat/httpsessionadapter.h"
#include <atomic>
#include <curl/curl.h>
#include <memory>


namespace configcat {

class LibCurlResourceGuard;

class CurlNetworkAdapter : public HttpSessionAdapter {
public:
    CurlNetworkAdapter() = default;
    ~CurlNetworkAdapter();

    bool init(uint32_t connectTimeoutMs, uint32_t readTimeoutMs) override;
    Response get(const std::string& url,
                 const std::map<std::string, std::string>& header,
                 const std::map<std::string, std::string>& proxies,
                 const std::map<std::string, ProxyAuthentication>& proxyAuthentications) override;
    void close() override;

private:
    // CURL progress functions
    int ProgressFunction(curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow);
    friend int ProgressCallback(void* clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow);

    std::shared_ptr<LibCurlResourceGuard> libCurlResourceGuard;
    CURL* curl = nullptr;
    std::atomic<bool> closed = false;
};

} // configcat

#endif // CONFIGCAT_EXTERNAL_NETWORK_ADAPTER_ENABLED
