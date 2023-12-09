add_executable(project2_main src/main.cpp)
target_link_libraries(project2_main
    PUBLIC
        PhysicalLayer
)

target_include_directories(project2_main
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include)



target_include_directories(PhysicalLayer
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(PhysicalLayer
    INTERFACE
        WASAPI
        ASIO
        Signal
        Boost
)

add_executable(naive-one src/naive-one.cpp)
target_link_libraries(naive-one
    PRIVATE
        PhysicalLayer
)

add_executable(naive-sender src/naive-sender.cpp)
target_link_libraries(naive-sender
    PRIVATE
        PhysicalLayer
)

add_executable(naive-receiver src/naive-receiver.cpp)
target_link_libraries(naive-receiver
    PRIVATE
        PhysicalLayer
)