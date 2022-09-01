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

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

option(CONFIGCAT_BUILD_TESTS "Build ConfigCat unittests." ON)

find_package(cpr REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(unofficial-hash-library REQUIRED)
find_package(semver REQUIRED)

set(CONFIGCAT_INCLUDE_PATHS
    "src"
    "include"
)

if(CONFIGCAT_BUILD_TESTS)
    include(CTest)
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
    # $<TARGET_PROPERTY:configcat,LINK_LIBRARIES> explicitly propagates private dependencies
    target_link_libraries(google_tests configcat gtest_main $<TARGET_PROPERTY:configcat,LINK_LIBRARIES>)

    gtest_discover_tests(google_tests)
endif()

file(GLOB SOURCES "src/*")
add_library(configcat ${SOURCES})
add_library(configcat::configcat ALIAS configcat)

set(CONFIGCAT_LIBRARIES ${CONFIGCAT_LIBRARIES} cpr::cpr nlohmann_json::nlohmann_json unofficial::hash-library semver::semver)

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

install(TARGETS configcat
    EXPORT configcat-targets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    FRAMEWORK DESTINATION ${CMAKE_INSTALL_PREFIX}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/configcat
)

export(EXPORT configcat-targets NAMESPACE configcat:: FILE configcat-targets.cmake)

set(CMAKE_INSTALL_CONFIGDIR ${CMAKE_INSTALL_LIBDIR}/cmake/configcat)
install(EXPORT configcat-targets
    DESTINATION ${CMAKE_INSTALL_CONFIGDIR}
    NAMESPACE configcat::
)

configure_package_config_file(${PROJECT_SOURCE_DIR}/cmake/configcat-config.cmake.in ${PROJECT_BINARY_DIR}/configcat-config.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_CONFIGDIR}
    PATH_VARS CMAKE_INSTALL_FULL_INCLUDEDIR
)

write_basic_package_version_file(${PROJECT_BINARY_DIR}/configcat-config-version.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES ${PROJECT_BINARY_DIR}/configcat-config.cmake ${PROJECT_BINARY_DIR}/configcat-config-version.cmake
    DESTINATION ${CMAKE_INSTALL_CONFIGDIR}
)
