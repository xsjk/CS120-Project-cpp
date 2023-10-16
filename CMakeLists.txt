cmake_minimum_required(VERSION 3.15)

project(CS120)

set(CMAKE_CXX_STANDARD 20)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

add_subdirectory(Libraries)
add_subdirectory(Applications)
