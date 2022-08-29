cmake_minimum_required(VERSION 3.14)
set(CMAKE_CXX_STANDARD 17)

project(configcat)

include(CTest)
include(FetchContent)

find_package(cpr REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(unofficial-hash-library REQUIRED)
FetchContent_Declare(
    cpp-semver
    GIT_REPOSITORY https://github.com/z4kn4fein/cpp-semver.git
    GIT_TAG v0.2.1
)
FetchContent_MakeAvailable(cpp-semver)

set(CONFIGCAT_INCLUDE_PATHS
    "src"
    "include"
)

if(BUILD_TESTING)
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
        ${tests}
        "${PROJECT_SOURCE_DIR}/test/test-main.cpp"
    )
    target_include_directories(google_tests PRIVATE ${CONFIGCAT_INCLUDE_PATHS})
    # $<TARGET_PROPERTY:configcat,LINK_LIBRARIES> explicitly propagates private dependencies
    target_link_libraries(google_tests configcat gtest_main $<TARGET_PROPERTY:configcat,LINK_LIBRARIES>)

    gtest_discover_tests(google_tests)
endif()

file(GLOB SOURCES "src/*" "third-party/hash-library/*")
add_library(configcat ${SOURCES})

set(CONFIGCAT_LIBRARIES ${CONFIGCAT_LIBRARIES} cpr::cpr nlohmann_json::nlohmann_json unofficial::hash-library semver)

target_link_libraries(configcat
    PRIVATE ${CONFIGCAT_LIBRARIES}
)

target_include_directories(configcat
    PUBLIC "include"
    PRIVATE ${CONFIGCAT_INCLUDE_PATHS}
)
