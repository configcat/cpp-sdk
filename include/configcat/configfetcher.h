#pragma once

#include <string>
#include <iostream>
#include <curl/curl.h>
#include "log.h"

namespace configcat {

class ConfigFetcher {
public:
    ConfigFetcher() {
        const std::string url("http://date.jsontest.com/");

        CURL* curl = curl_easy_init();

        // Set remote URL.
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

        // Don't bother trying IPv6, which would increase DNS resolution time.
        curl_easy_setopt(curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);

        // Don't wait forever, time out after 10 seconds.
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10);

        // Follow HTTP redirects if necessary.
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

        // Response information.
        long httpCode(0);
        std::unique_ptr<std::string> httpData(new std::string());

        // Hook up data handling function.
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, ConfigFetcher::callback);

        // Hook up data container (will be passed as the last parameter to the
        // callback handling function).  Can be any pointer type, since it will
        // internally be passed as a void pointer.
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, httpData.get());

        // Run our HTTP GET command, capture the HTTP response code, and clean up.
        curl_easy_perform(curl);
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
        curl_easy_cleanup(curl);

        if (httpCode == 200) {
            LOG_INFO << "Got successful response from " << url;
            LOG_INFO << *httpData;

            code = httpCode;
            data = *httpData;
        } else {
            LOG_ERROR << "Couldn't GET from " << url << " - exiting";
        }
    }

    long code = 0;
    std::string data = "";

private:
    static std::size_t callback(const char* in, std::size_t size, std::size_t num, std::string* out) {
        const std::size_t totalBytes(size * num);
        out->append(in, totalBytes);
        return totalBytes;
    }
};

} // namespace configcat
