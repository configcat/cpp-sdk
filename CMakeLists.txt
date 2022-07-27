cmake_minimum_required(VERSION 3.14)
set(CMAKE_CXX_STANDARD 17)

# vcpkg init
#set(vcpkg "$ENV{VCPKG_INSTALLATION_ROOT}/scripts/buildsystems/vcpkg.cmake")
#set(vcpkg "$ENV{HOME}/git/cat/vcpkg/scripts/buildsystems/vcpkg.cmake")
#message(STATUS "vcpkg : ${vcpkg}")
message(STATUS "CMAKE_TOOLCHAIN_FILE: ${CMAKE_TOOLCHAIN_FILE}")
#if(NOT CMAKE_TOOLCHAIN_FILE AND EXISTS "${vcpkg}")
#    set(CMAKE_TOOLCHAIN_FILE "${vcpkg}" CACHE FILEPATH "CMake toolchain file")
#    message(STATUS "vcpkg toolchain found: ${CMAKE_TOOLCHAIN_FILE}")
#endif()

project(configcat)

include(CTest)

# TODO: remove after vcpkg fix
#set(RAPIDJSON_INCLUDE_DIR "$ENV{HOME}/Downloads/rapidjson-1.1.0/include")
#message(STATUS "rapidjson: ${RAPIDJSON_INCLUDE_DIR}")

# TODO: nlohmann json
#set(NLOHMANNJSON_INCLUDE_DIR "$ENV{HOME}/Downloads/json/single_include")
#message(STATUS "nlohmannjson: ${NLOHMANNJSON_INCLUDE_DIR}")


#include(FetchContent)
#FetchContent_Declare(cpr GIT_REPOSITORY https://github.com/libcpr/cpr.git
#        GIT_TAG 871ed52d350214a034f6ef8a3b8f51c5ce1bd400) # The commit hash for 1.9.0. Replace with the latest from: https://github.com/libcpr/cpr/releases
#set(CPR_USE_SYSTEM_GTEST ON CACHE BOOL "" FORCE)
#set(CPR_BUILD_TESTS OFF CACHE INTERNAL "" FORCE)
#FetchContent_MakeAvailable(cpr)

# Find ConfigCat dependencies.
#find_package(CURL REQUIRED)
#find_package(RapidJSON REQUIRED)

#find_package(CPR REQUIRED)
#find_package(cpr CONFIG REQUIRED)
find_package(cpr REQUIRED)
find_package(nlohmann_json REQUIRED)

#find_path(CPR_INCLUDE_DIR cpr.h)
#find_library(CPR_LIBRARY cpr)


#get_cmake_property(_variableNames VARIABLES)
#list (SORT _variableNames)
#foreach (_variableName ${_variableNames})
#    message(STATUS "${_variableName}=${${_variableName}}")
#endforeach()




set(CONFIGCAT_INCLUDE_PATHS
    "src"
    "include"
#    ${CURL_INCLUDE_DIRS}
#    ${RAPIDJSON_INCLUDE_DIR}
#    ${NLOHMANNJSON_INCLUDE_DIR}
)

#message(STATUS "CPR_INCLUDE_DIR: ${CPR_INCLUDE_DIR}")
#message(STATUS "CPR_LIBRARY: ${CPR_LIBRARY}")
#
#message(STATUS "CPR_INCLUDE_DIRS: ${CPR_INCLUDE_DIRS}")
#message(STATUS "CPR_LIBRARIES: ${CPR_LIBRARIES}")


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

#set(CONFIGCAT_LIBRARIES ${CONFIGCAT_LIBRARIES} ${CURL_LIBRARIES})
set(CONFIGCAT_LIBRARIES ${CONFIGCAT_LIBRARIES} cpr::cpr nlohmann_json::nlohmann_json)

target_link_libraries(configcat
#    PUBLIC  ${CONFIGCAT_PUBLIC_LIBRARIES}
#    PRIVATE ${CURL_LIBRARIES}
    PRIVATE ${CONFIGCAT_LIBRARIES}
)

target_include_directories(configcat
    PUBLIC "include"
    PRIVATE ${CONFIGCAT_INCLUDE_PATHS}
)

