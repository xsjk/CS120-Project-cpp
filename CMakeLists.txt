cmake_minimum_required(VERSION 3.15)

project(CS120 VERSION 0.0.1)

if(NOT MSVC)
    message(FATAL_ERROR "Only MSVC is supported")
endif()

add_subdirectory(Libraries)
add_subdirectory(Applications)
