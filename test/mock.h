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
    configcat::EvaluationDetails<> evaluationDetails;
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

    void onFlagEvaluated(const configcat::EvaluationDetailsBase& details) {
        evaluationDetails = to_concrete(details);
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
    "key1": {
      "t": 0,
      "v": {
        "b": true
      },
      "i": "fakeId1"
    },
    "key2": {
      "t": 0,
      "v": {
        "b": false
      },
      "i": "fakeId2"
    },
    "testBoolKey": {
      "t": 0,
      "v": {
        "b": true
      }
    },
    "testDoubleKey": {
      "t": 3,
      "v": {
        "d": 1.1
      }
    },
    "testIntKey": {
      "t": 2,
      "v": {
        "i": 1
      }
    },
    "testStringKey": {
      "t": 1,
      "r": [
        {
          "c": [
            {
              "u": {
                "a": "Identifier",
                "c": 2,
                "l": [
                  "@test1.com"
                ]
              }
            }
          ],
          "s": {
            "v": {
              "s": "fake1"
            },
            "i": "id1"
          }
        },
        {
          "c": [
            {
              "u": {
                "a": "Identifier",
                "c": 2,
                "l": [
                  "@test2.com"
                ]
              }
            }
          ],
          "s": {
            "v": {
              "s": "fake2"
            },
            "i": "id2"
          }
        }
      ],
      "v": {
        "s": "testValue"
      },
      "i": "id"
    }
  }
})";
