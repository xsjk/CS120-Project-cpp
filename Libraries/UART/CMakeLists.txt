cmake_minimum_required(VERSION 3.15)

file(GLOB SOURCES "*.cpp")

add_library(UART SHARED ${SOURCES})

target_include_directories(UART PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(UART PUBLIC ASIO)
target_compile_definitions(UART PRIVATE UART_EXPORTS)
