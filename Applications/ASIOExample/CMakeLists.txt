file(GLOB SOURCES *.cpp)
add_executable(asio_example ${SOURCES})
target_link_libraries(asio_example PRIVATE ASIO)
