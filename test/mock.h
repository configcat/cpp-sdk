#pragma once

#include <queue>

#include "configcat/configcatcache.h"
#include "configcat/httpsessionadapter.h"

class MockConfigCatCache : public configcat::ConfigCatCache {
public:
    MockConfigCatCache(const std::string& cache): cache(cache) {}

    const std::string& read(const std::string& key) override {
        return cache;
    }

    void write(const std::string& key, const std::string& value) override {
        // Do nothing
    }

    std::string cache;
};

class MockHttpSessionAdapter : public configcat::HttpSessionAdapter {
public:
    void enqueueResponse(const configcat::Response& response) {
        responses.push(response);
    }

    virtual configcat::Response get(const std::string& url) override {
        requests.emplace_back(url);

        if (!responses.empty()) {
            auto response = responses.front();
            responses.pop();
            return response;
        }

        return {};
    };

    std::queue<configcat::Response> responses;
    std::vector<std::string> requests;
};
