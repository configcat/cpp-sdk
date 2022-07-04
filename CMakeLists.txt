cmake_minimum_required(VERSION 3.14)
project(configcat)

set(CMAKE_CXX_STANDARD 14)

include(CTest)

if(BUILD_TESTING)
    include(FetchContent)
    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/release-1.12.1.zip
    )
    # For Windows: Prevent overriding the parent project's compiler/linker settings
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    enable_testing()
    include(GoogleTest)

    include_directories("${PROJECT_SOURCE_DIR}/include")

    file(GLOB tests "${PROJECT_SOURCE_DIR}/test/*.cpp")
    list(REMOVE_ITEM tests "${PROJECT_SOURCE_DIR}/test/test-main.cpp")

    add_executable("google_tests"
        ${sources}
        ${tests}
        "${PROJECT_SOURCE_DIR}/test/test-main.cpp"
    )
    target_include_directories(google_tests PRIVATE ${CONFIGCAT_INCLUDE_PATHS})
    target_link_libraries(google_tests configcat gtest_main)

    gtest_discover_tests(google_tests)
endif()

set(CONFIGCAT_INCLUDE_PATHS
    "src"
    "include"
    "third-party/include"
)

add_library(configcat src/configcat.cpp)
