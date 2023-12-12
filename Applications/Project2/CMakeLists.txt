add_library(project2_lib INTERFACE)

target_include_directories(project2_lib
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(project2_lib
    INTERFACE
        Boost
        Utils
        project1_lib
)

add_executable(project2_main src/main.cpp)
target_link_libraries(project2_main
    PUBLIC
        project2_lib
)


add_executable(naive-one src/naive-one.cpp)
target_link_libraries(naive-one
    PRIVATE
        project2_lib
)

add_executable(naive-sender src/naive-sender.cpp)
target_link_libraries(naive-sender
    PRIVATE
        project2_lib
)

add_executable(naive-receiver src/naive-receiver.cpp)
target_link_libraries(naive-receiver
    PRIVATE
        project2_lib
)
