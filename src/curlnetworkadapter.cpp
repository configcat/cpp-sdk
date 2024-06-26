#ifndef CONFIGCAT_EXTERNAL_NETWORK_ADAPTER_ENABLED

#include "curlnetworkadapter.h"
#include <mutex>
#include <sstream>
#include <vector>

namespace configcat {

class LibCurlResourceGuard {
private:
    static std::shared_ptr<LibCurlResourceGuard> instance;
    static std::mutex mutex;

    LibCurlResourceGuard() {
        curl_global_init(CURL_GLOBAL_DEFAULT);
    }

    struct Deleter {
        void operator()(LibCurlResourceGuard* libCurlResourceGuard) {
            curl_global_cleanup();
            delete libCurlResourceGuard;
            LibCurlResourceGuard::instance.reset();
        }
    };

public:
    // Prevent copying
    LibCurlResourceGuard(const LibCurlResourceGuard&) = delete;
    LibCurlResourceGuard& operator=(const LibCurlResourceGuard&) = delete;

    static std::shared_ptr<LibCurlResourceGuard> getInstance() {
        std::lock_guard<std::mutex> lock(mutex);
        if (!instance) {
            instance = std::shared_ptr<LibCurlResourceGuard>(new LibCurlResourceGuard(), Deleter());
        }
        return instance;
    }
};
std::shared_ptr<LibCurlResourceGuard> LibCurlResourceGuard::instance = nullptr;
std::mutex LibCurlResourceGuard::mutex;

int ProgressCallback(void* clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow) {
    if (clientp)
        return static_cast<CurlNetworkAdapter*>(clientp)->ProgressFunction(dltotal, dlnow, ultotal, ulnow);
    return 1; // Return abort
}

int CurlNetworkAdapter::ProgressFunction(curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow) {
    return closed ? 1 : 0;  // Return 0 to continue, or 1 to abort
}

static size_t WriteCallback(void *contents, size_t size, size_t nmemb, std::string *userp) {
    userp->append((char*)contents, size * nmemb);
    return size * nmemb;
}

std::map<std::string, std::string> ParseHeader(const std::string& headerString) {
    std::map<std::string, std::string> header;

    std::vector<std::string> lines;
    std::istringstream stream(headerString);
    std::string line;
    while (std::getline(stream, line, '\n')) {
        lines.push_back(line);
    }

    for (std::string& line : lines) {
        if (line.length() > 0) {
            const size_t colonIndex = line.find(':');
            if (colonIndex != std::string::npos) {
                std::string value = line.substr(colonIndex + 1);
                value.erase(0, value.find_first_not_of("\t "));
                value.resize(std::min<size_t>(value.size(), value.find_last_not_of("\t\n\r ") + 1));
                header[line.substr(0, colonIndex)] = value;
            }
        }
    }

    return header;
}

bool CurlNetworkAdapter::init(uint32_t connectTimeoutMs, uint32_t readTimeoutMs) {
    libCurlResourceGuard = LibCurlResourceGuard::getInstance();
    curl = curl_easy_init();
    if (!curl) {
        return false;
    }

    // Timeout setup
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, connectTimeoutMs);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, readTimeoutMs);

    // Enable the progress callback function to be able to abort the request
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, ProgressCallback);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, this);

    return true;
}

Response CurlNetworkAdapter::get(const std::string& url,
                                 const std::map<std::string, std::string>& header,
                                 const std::map<std::string, std::string>& proxies,
                                 const std::map<std::string, ProxyAuthentication>& proxyAuthentications) {
    Response response;
    if (!curl) {
        response.errorCode = ResponseErrorCode::InternalError;
        response.error = "CURL is not initialized.";
        return response;
    }

    CURLcode res;
    std::string headerString;

    // Update header
    struct curl_slist* headers = NULL;
    for (const auto& it : header) {
        headers = curl_slist_append(headers, (it.first + ": " + it.second).c_str());
    }

    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

    // Set the callback function to receive the response
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response.text);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &headerString);

    // Proxy setup
    const std::string protocol = url.substr(0, url.find(':'));
    if (proxies.count(protocol) > 0) {
        curl_easy_setopt(curl, CURLOPT_PROXY, proxies.at(protocol).c_str());
        if (proxyAuthentications.count(protocol) > 0) {
            curl_easy_setopt(curl, CURLOPT_PROXYUSERNAME, proxyAuthentications.at(protocol).user.c_str());
            curl_easy_setopt(curl, CURLOPT_PROXYPASSWORD, proxyAuthentications.at(protocol).password.c_str());
        }
    }

    // Perform the request
    res = curl_easy_perform(curl);

    if (res != CURLE_OK) {
        response.error = curl_easy_strerror(res);
        if (res == CURLE_OPERATION_TIMEDOUT) {
            response.errorCode = ResponseErrorCode::TimedOut;
        } else if (res == CURLE_ABORTED_BY_CALLBACK) {
            response.errorCode = ResponseErrorCode::RequestCancelled;
        } else {
            response.errorCode = ResponseErrorCode::InternalError;
        }
        return response;
    }

    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response.statusCode);

    // Parse the headers from headerString
    response.header = ParseHeader(headerString);

    return response;
}

void CurlNetworkAdapter::close() {
    closed = true;
}

CurlNetworkAdapter::~CurlNetworkAdapter() {
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_XFERINFODATA, NULL);
        curl_easy_cleanup(curl);
    }
}

} // configcat

#endif // CONFIGCAT_EXTERNAL_NETWORK_ADAPTER_ENABLED
