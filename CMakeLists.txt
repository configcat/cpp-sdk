cmake_minimum_required(VERSION 3.14)
set(CMAKE_CXX_STANDARD 17)

# vcpkg init
set(vcpkg "$ENV{VCPKG_INSTALLATION_ROOT}/scripts/buildsystems/vcpkg.cmake")
if(NOT CMAKE_TOOLCHAIN_FILE AND EXISTS "${vcpkg}")
    set(CMAKE_TOOLCHAIN_FILE "${vcpkg}" CACHE FILEPATH "CMake toolchain file")
    message(STATUS "vcpkg toolchain found: ${CMAKE_TOOLCHAIN_FILE}")
endif()

project(configcat)

include(CTest)

# TODO: remove after vcpkg fix
set(RAPIDJSON_INCLUDE_DIR "$ENV{HOME}/Downloads/rapidjson-1.1.0/include")
message(STATUS "rapidjson: ${RAPIDJSON_INCLUDE_DIR}")

# Find ConfigCat dependencies.
find_package(CURL REQUIRED)
#find_package(RapidJSON REQUIRED)

set(CONFIGCAT_INCLUDE_PATHS
    "src"
    "include"
    ${CURL_INCLUDE_DIR}
    ${RAPIDJSON_INCLUDE_DIR}
)

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
        ${tests}
        "${PROJECT_SOURCE_DIR}/test/test-main.cpp"
    )
    target_include_directories(google_tests PRIVATE ${CONFIGCAT_INCLUDE_PATHS})
    target_link_libraries(google_tests configcat gtest_main)

    gtest_discover_tests(google_tests)
endif()

file(GLOB SOURCES "src/*")
add_library(configcat ${SOURCES})

set(CONFIGCAT_LIBRARIES ${CONFIGCAT_LIBRARIES} ${CURL_LIBRARIES})

target_link_libraries(configcat
#    PUBLIC  ${CONFIGCAT_PUBLIC_LIBRARIES}
    PRIVATE ${CURL_LIBRARIES}
)

target_include_directories(configcat
    PUBLIC "include"
    PRIVATE ${CONFIGCAT_INCLUDE_PATHS}
)

