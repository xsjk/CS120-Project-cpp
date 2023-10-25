file(GLOB SOURCES *.cpp)
add_executable(asio_audio_example ${SOURCES})
target_link_libraries(asio_audio_example PRIVATE ASIO)
