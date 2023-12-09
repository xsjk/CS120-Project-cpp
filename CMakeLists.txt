cmake_minimum_required(VERSION 3.15)

project(CS120)

set(CMAKE_CXX_STANDARD 20)

set(CMAKE_BINARY_DIR ${CMAKE_SOURCE_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

if(MSVC)
    # Make sure that the shared library has its symbols exported to .lib file.
    # Otherwise MSVC will not generate .lib file and we will end up with linker errors 
    # when compiling some executables that depend on the shared library.
    set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif()

message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")

add_subdirectory(Libraries)
add_subdirectory(Applications)
