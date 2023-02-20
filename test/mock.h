#pragma once

#include <queue>
#include <thread>
#include <chrono>
#include <unordered_map>
#include <string>

#include "configcat/configcache.h"
#include "configcat/httpsessionadapter.h"

class InMemoryConfigCache : public configcat::ConfigCache {
public:
    InMemoryConfigCache() {}

    const std::string& read(const std::string& key) override {
        return store[key];
    }

    void write(const std::string& key, const std::string& value) override {
        store[key] = value;
    }

    std::unordered_map<std::string, std::string> store;
};

class MockHttpSessionAdapter : public configcat::HttpSessionAdapter {
public:
    struct Request {
        std::string url;
        std::map<std::string, std::string> header;
    };

    struct MockResponse {
        configcat::Response response;
        int delayInSeconds = 0;
    };

    void enqueueResponse(const configcat::Response& response, int delayInSeconds = 0) {
        responses.push({response, delayInSeconds});
    }

    configcat::Response get(const std::string& url, const std::map<std::string, std::string>& header) override {
        requests.push_back({url, header});

        if (!responses.empty()) {
            auto mockResponse = responses.front();
            if (mockResponse.delayInSeconds > 0) {
                constexpr int stepMilliseconds = 100;
                for (int delay = 0; delay < mockResponse.delayInSeconds * 1000; delay += stepMilliseconds) {
                    if (closed) {
                        constexpr int closedByClientStatusCode = 499;
                        return configcat::Response{closedByClientStatusCode, ""};
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(stepMilliseconds));
                }
            }

            responses.pop();
            return mockResponse.response;
        }

        return {};
    };

    void close() override {
        closed = true;
    }

    std::queue<MockResponse> responses;
    std::vector<Request> requests;

private:
    std::atomic<bool> closed = false;
};
