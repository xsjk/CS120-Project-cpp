file(GLOB SOURCES *.cpp)
add_executable(uart_example ${SOURCES})
target_link_libraries(uart_example PRIVATE UART)
