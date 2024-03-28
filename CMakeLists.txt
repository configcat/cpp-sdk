cmake_minimum_required(VERSION 3.14)
set(CMAKE_CXX_STANDARD 17)

file(READ "src/version.h" HeaderVersionFile)
string(REGEX MATCH
    "#define CONFIGCAT_VERSION \"([0-9\\.]+).*\""
    _
    ${HeaderVersionFile}
)
set(CONFIGCAT_VERSION ${CMAKE_MATCH_1})
message("ConfigCat version: ${CONFIGCAT_VERSION}")

project(configcat VERSION ${CONFIGCAT_VERSION})

include(FetchContent)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

option(CONFIGCAT_USE_EXTERNAL_NETWORK_ADAPTER "Use external network adapter." OFF)
option(CONFIGCAT_BUILD_TESTS "Build ConfigCat unittests." ON)

if(NOT CONFIGCAT_USE_EXTERNAL_NETWORK_ADAPTER)
    find_package(CURL REQUIRED)
endif()

find_package(nlohmann_json REQUIRED)
find_package(unofficial-hash-library REQUIRED)
find_package(semver REQUIRED)

set(CONFIGCAT_INCLUDE_PATHS
    "src"
    "include"
)

if(CONFIGCAT_BUILD_TESTS)
    include(CTest)

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
    target_link_libraries(google_tests configcat gmock_main $<TARGET_PROPERTY:configcat,LINK_LIBRARIES>)

    gtest_discover_tests(google_tests)
endif()

file(GLOB SOURCES "src/*")
add_library(configcat ${SOURCES})
add_library(configcat::configcat ALIAS configcat)

if(NOT CONFIGCAT_USE_EXTERNAL_NETWORK_ADAPTER)
    set(CONFIGCAT_LIBRARIES ${CONFIGCAT_LIBRARIES} CURL::libcurl nlohmann_json::nlohmann_json unofficial::hash-library semver::semver)
else()
    set(CONFIGCAT_LIBRARIES ${CONFIGCAT_LIBRARIES} nlohmann_json::nlohmann_json unofficial::hash-library semver::semver)
    target_compile_definitions(configcat PRIVATE CONFIGCAT_EXTERNAL_NETWORK_ADAPTER_ENABLED)
endif()

target_link_libraries(configcat
    PRIVATE ${CONFIGCAT_LIBRARIES}
)

target_include_directories(${PROJECT_NAME}
    PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
    ${CONFIGCAT_INCLUDE_PATHS}
)

# install

configure_package_config_file(
        ${PROJECT_SOURCE_DIR}/cmake/${PROJECT_NAME}-config.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}-config.cmake
        INSTALL_DESTINATION ${CMAKE_INSTALL_DATADIR}/cmake/${PROJECT_NAME}
        PATH_VARS CMAKE_INSTALL_FULL_INCLUDEDIR)

write_basic_package_version_file(
        ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}-config-version.cmake
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion
        ARCH_INDEPENDENT)

install(DIRECTORY ${PROJECT_SOURCE_DIR}/include/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}-config-version.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}-config.cmake
        DESTINATION ${CMAKE_INSTALL_DATADIR}/cmake/${PROJECT_NAME})

export(TARGETS ${PROJECT_NAME}
        NAMESPACE ${PROJECT_NAME}::
        FILE ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}-targets.cmake)

install(TARGETS ${PROJECT_NAME}
        EXPORT ${PROJECT_NAME}-targets
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(EXPORT ${PROJECT_NAME}-targets
        NAMESPACE ${PROJECT_NAME}::
        DESTINATION ${CMAKE_INSTALL_DATADIR}/cmake/${PROJECT_NAME})
