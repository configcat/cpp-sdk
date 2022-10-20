cmake_minimum_required(VERSION 3.14)
set(CMAKE_CXX_STANDARD 17)

message(STATUS "CMAKE_TOOLCHAIN_FILE: ${CMAKE_TOOLCHAIN_FILE}")

project(example C CXX)

add_executable(example main.cpp)

find_package(configcat REQUIRED)

target_link_libraries(example PRIVATE configcat::configcat)
