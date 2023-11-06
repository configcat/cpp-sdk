#include "configfetcher.h"

#include "configcat/log.h"
#include "configcat/httpsessionadapter.h"
#include "configcat/configcatoptions.h"
#include "configcat/configcatlogger.h"
#include "version.h"
#include "platform.h"
#include "utils.h"

using namespace std;

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

int ProgressCallback(void *clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow) {
    return static_cast<ConfigFetcher*>(clientp)->ProgressFunction(dltotal, dlnow, ultotal, ulnow);
}

int ConfigFetcher::ProgressFunction(curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow) {
    return closed ? 1 : 0;  // Return 0 to continue, or 1 to abort
}

ConfigFetcher::ConfigFetcher(const string& sdkKey, shared_ptr<ConfigCatLogger> logger, const string& mode, const ConfigCatOptions& options):
    sdkKey(sdkKey),
    logger(logger),
    mode(mode),
    connectTimeoutMs(options.connectTimeoutMs),
    readTimeoutMs(options.readTimeoutMs),
    proxies(options.proxies),
    proxyAuthentications(options.proxyAuthentications),
    httpSessionAdapter(options.httpSessionAdapter) {
    urlIsCustom = !options.baseUrl.empty();
    url = urlIsCustom
        ? options.baseUrl
        : options.dataGovernance == DataGovernance::Global
            ? kGlobalBaseUrl
            : kEuOnlyBaseUrl;
    userAgent = string("ConfigCat-Cpp/") + mode + "-" + CONFIGCAT_VERSION;

    libCurlResourceGuard = LibCurlResourceGuard::getInstance();
    curl = curl_easy_init();
    if (!curl) {
        LOG_ERROR(0) << "Cannot initialize CURL.";
        return;
    }

    // Timeout setup
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, connectTimeoutMs);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, readTimeoutMs);

    // Enable the progress callback function to be able to abort the request
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, ProgressCallback);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, this);
}

ConfigFetcher::~ConfigFetcher() {
    if (curl) {
        curl_easy_cleanup(curl);
    }
}

void ConfigFetcher::close() {
    if (httpSessionAdapter) {
        httpSessionAdapter->close();
    }
    closed = true;
}

FetchResponse ConfigFetcher::fetchConfiguration(const std::string& eTag) {
    return executeFetch(eTag, 2);
}

FetchResponse ConfigFetcher::executeFetch(const std::string& eTag, int executeCount) {
    auto response = fetch(eTag);
    auto preferences = response.entry && response.entry->config ? response.entry->config->preferences : nullptr;

    // If there wasn't a config change or there were no preferences in the config, we return the response
    if (!response.isFetched() || preferences == nullptr) {
        return response;
    }

    // If the preferences url is the same as the last called one, just return the response.
    if (!preferences->url.empty() && url == preferences->url) {
        return response;
    }

    // If the url is overridden, and the redirect parameter is not ForceRedirect,
    // the SDK should not redirect the calls, and it just has to return the response.
    if (urlIsCustom && preferences->redirect != ForceRedirect) {
        return response;
    }

    // The next call should use the preferences url provided in the config json
    url = preferences->url;

    if (preferences->redirect == NoRedirect) {
        return response;
    }

    // Try to download again with the new url

    if (preferences->redirect == ShouldRedirect) {
        LOG_WARN(3002) <<
            "The `dataGovernance` parameter specified at the client initialization is not in sync with the preferences on the ConfigCat Dashboard. "
            "Read more: https://configcat.com/docs/advanced/data-governance/";
    }

    if (executeCount > 0) {
        return executeFetch(eTag, executeCount - 1);
    }

    LOG_ERROR(1104) << "Redirection loop encountered while trying to fetch config JSON. Please contact us at https://configcat.com/support/";
    return response;
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

FetchResponse ConfigFetcher::fetch(const std::string& eTag) {
    string requestUrl(url + "/configuration-files/" + sdkKey + "/" + kConfigJsonName);
    std::map<std::string, std::string> responseHeaders;
    std::string readBuffer;
    long response_code = 0;

    if (!httpSessionAdapter) {
        if (!curl) {
            LogEntry logEntry = LogEntry(logger, LOG_LEVEL_ERROR, 1103) <<
                "Unexpected error occurred while trying to fetch config JSON: CURL is not initialized.";
            return FetchResponse(failure, ConfigEntry::empty, logEntry.getMessage(), true);
        }

        CURLcode res;
        std::string headerString;

        // Update header
        struct curl_slist* headers = NULL;
        headers = curl_slist_append(headers, (string(kUserAgentHeaderName) + ": " + userAgent).c_str());
        headers = curl_slist_append(headers, (string(kPlatformHeaderName) + ": " + getPlatformName()).c_str());
        if (!eTag.empty()) {
            headers = curl_slist_append(headers, (string(kIfNoneMatchHeaderName) + ": " + eTag).c_str());
        }

        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_URL, requestUrl.c_str());

        // Set the callback function to receive the response
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
        curl_easy_setopt(curl, CURLOPT_HEADERDATA, &headerString);

        // Proxy setup
        const std::string protocol = url.substr(0, url.find(':'));
        if (proxies.count(protocol) > 0) {
            curl_easy_setopt(curl, CURLOPT_PROXY, proxies[protocol].c_str());
            if (proxyAuthentications.count(protocol) > 0) {
                curl_easy_setopt(curl, CURLOPT_PROXYUSERNAME, proxyAuthentications[protocol].user.c_str());
                curl_easy_setopt(curl, CURLOPT_PROXYPASSWORD, proxyAuthentications[protocol].password.c_str());
            }
        }

        // Perform the request
        res = curl_easy_perform(curl);

        if (res != CURLE_OK) {
            LogEntry logEntry = (res == CURLE_OPERATION_TIMEDOUT)
                                ? LogEntry(logger, LOG_LEVEL_ERROR, 1102) <<
                                    "Request timed out while trying to fetch config JSON. "
                                    "Timeout values: [connect: " << connectTimeoutMs << "ms, read: " << readTimeoutMs << "ms]"
                                : LogEntry(logger, LOG_LEVEL_ERROR, 1103) <<
                                    "Unexpected error occurred while trying to fetch config JSON: " << curl_easy_strerror(res);

            return FetchResponse(failure, ConfigEntry::empty, logEntry.getMessage(), true);
        }

        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);

        // Parse the headers from headerString
        responseHeaders = ParseHeader(headerString);
    } else {
        std::map<std::string, std::string> requestHeader = {
            {kUserAgentHeaderName, userAgent},
            {kPlatformHeaderName, getPlatformName()}
        };
        if (!eTag.empty()) {
            requestHeader.insert({kIfNoneMatchHeaderName, eTag});
        }
        auto response = httpSessionAdapter->get(requestUrl, requestHeader);
        response_code = response.status_code;
        readBuffer = response.text;
        responseHeaders = response.header;
    }

    switch (response_code) {
        case 200:
        case 201:
        case 202:
        case 203:
        case 204: {
            const auto it = responseHeaders.find(kEtagHeaderName);
            string eTag = it != responseHeaders.end() ? it->second : "";
            try {
                auto config = Config::fromJson(readBuffer);
                LOG_DEBUG << "Fetch was successful: new config fetched.";
                return FetchResponse(fetched, make_shared<ConfigEntry>(config, eTag, readBuffer, getUtcNowSecondsSinceEpoch()));
            } catch (exception& exception) {
                LogEntry logEntry(logger, LOG_LEVEL_ERROR, 1105);
                logEntry <<
                    "Fetching config JSON was successful but the HTTP response content was invalid. "
                    "Config JSON parsing failed. " << exception.what();
                return FetchResponse(failure, ConfigEntry::empty, logEntry.getMessage(), true);
            }
        }

        case 304:
            LOG_DEBUG << "Fetch was successful: config not modified.";
            return FetchResponse({notModified, ConfigEntry::empty});

        case 403:
        case 404: {
            LogEntry logEntry(logger, LOG_LEVEL_ERROR, 1100);
            logEntry <<
                "Your SDK Key seems to be wrong. You can find the valid SDK Key at https://app.configcat.com/sdkkey. "
                "Received unexpected response: " << response_code;
            return FetchResponse(failure, ConfigEntry::empty, logEntry.getMessage(), false);
        }

        default: {
            LogEntry logEntry(logger, LOG_LEVEL_ERROR, 1101);
            logEntry << "Unexpected HTTP response was received while trying to fetch config JSON: " << response_code;
            return FetchResponse(failure, ConfigEntry::empty, logEntry.getMessage(), true);
        }
    }
}

} // namespace configcat
