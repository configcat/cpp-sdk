# set(HTTPLIB_REQUIRE_OPENSSL ON)
# set(HTTPLIB_COMPILE ON)
# set(BUILD_SHARED_LIBS ON)
set(BUILD_CURL_EXE OFF)

FetchContent_Declare(curl
    GIT_REPOSITORY    https://github.com/curl/curl.git
    GIT_TAG           curl-8_11_0
)

FetchContent_MakeAvailable(curl)
