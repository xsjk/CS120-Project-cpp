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
)

add_executable(naive-one src/naive-one.cpp)
target_link_libraries(naive-one
    PRIVATE
        PhysicalLayer
)