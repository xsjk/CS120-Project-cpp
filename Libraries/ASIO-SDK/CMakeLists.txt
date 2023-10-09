cmake_minimum_required(VERSION 3.15)
project(ASIO-SDK)

file(GLOB COMMON_SOURCES "common/*.cpp")
file(GLOB HOST_SOURCES "host/*.cpp")
file(GLOB HOST_PC_SOURCES "host/pc/*.cpp")
list(REMOVE_ITEM COMMON_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/common/asiodrvr.cpp")

add_library(ASIO STATIC ${COMMON_SOURCES} ${HOST_SOURCES} ${HOST_PC_SOURCES})

# for clang
find_package(Threads REQUIRED)
target_link_libraries(ASIO ${CMAKE_THREAD_LIBS_INIT})

target_include_directories(ASIO PUBLIC common)
target_include_directories(ASIO PUBLIC host)
target_include_directories(ASIO PUBLIC host/pc)
