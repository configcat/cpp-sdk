set(HTTPLIB_REQUIRE_OPENSSL ON)
set(HTTPLIB_COMPILE ON)
set(BUILD_SHARED_LIBS ON)

FetchContent_Declare(httplib
    GIT_REPOSITORY    https://github.com/yhirose/cpp-httplib.git
    GIT_TAG           v0.15.3
)

FetchContent_MakeAvailable(httplib)
