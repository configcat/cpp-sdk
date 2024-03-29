#pragma once

#include <queue>
#include <thread>
#include <chrono>
#include <unordered_map>
#include <string>

#include "configcat/configcache.h"
#include "configcat/httpsessionadapter.h"
#include "configcat/config.h"
#include "configcat/evaluationdetails.h"

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

class SingleValueCache : public configcat::ConfigCache {
public:
    SingleValueCache(const std::string& value): value(value) {}

    const std::string& read(const std::string& key) override {
        return value;
    }

    void write(const std::string& key, const std::string& value) override {
        this->value = value;
    }

    std::string value;
};

class HookCallbacks {
public:
    bool isReady = false;
    int isReadyCallCount = 0;
    std::shared_ptr<configcat::Settings> changedConfig;
    int changedConfigCallCount = 0;
    configcat::EvaluationDetails evaluationDetails;
    int evaluationDetailsCallCount = 0;
    std::string error;
    int errorCallCount = 0;

    void onClientReady() {
        isReady = true;
        isReadyCallCount += 1;
    }

    void onConfigChanged(std::shared_ptr<configcat::Settings> config) {
        changedConfig = config;
        changedConfigCallCount += 1;
    }

    void onFlagEvaluated(const configcat::EvaluationDetails& details) {
        evaluationDetails = details;
        evaluationDetailsCallCount += 1;
    }

    void onError(const std::string& error) {
        this->error = error;
        errorCallCount += 1;
    }
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

    bool init(uint32_t connectTimeoutMs, uint32_t readTimeoutMs) override {
        return true;
    }

    configcat::Response get(const std::string& url, const std::map<std::string, std::string>& header,
                            const std::map<std::string, std::string>& proxies,
                            const std::map<std::string, configcat::ProxyAuthentication>& proxyAuthentications) override {
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

static constexpr char kTestJsonString[] = R"({
    "p": {
        "u": "https://cdn-global.configcat.com",
        "r": 0
    },
    "f": {
        "testBoolKey": {
            "v": true, "t": 0, "p": [], "r": []
        },
        "testStringKey": {
            "v": "testValue", "i": "id", "t": 1, "p": [],
            "r": [
                {
                    "i": "id1", "v": "fake1", "a": "Identifier", "t": 2, "c": "@test1.com"
                },
                {
                    "i": "id2", "v": "fake2", "a": "Identifier", "t": 2, "c": "@test2.com"
                }
            ]
        },
        "testIntKey": {
            "v": 1, "t": 2, "p": [], "r": []
        },
        "testDoubleKey": {
            "v": 1.1, "t": 3, "p": [], "r": []
        },
        "key1": {
            "v": true, "i": "fakeId1", "p": [], "r": []
        },
        "key2": {
            "v": false, "i": "fakeId2", "p": [], "r": []
        }
    }
})";
