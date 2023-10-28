cmake_minimum_required(VERSION 3.15)

add_library(Signal INTERFACE ${SOURCES})

target_include_directories(Signal INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
