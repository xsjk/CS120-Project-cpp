file(GLOB SOURCES *.cpp)
add_executable(player_example ${SOURCES})
target_link_libraries(player_example PRIVATE ASIO)
