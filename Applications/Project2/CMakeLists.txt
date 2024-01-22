add_library(project2_lib INTERFACE)

target_link_libraries(project2_lib
    INTERFACE
        Boost
        utils
        project1_lib
)

add_executable(project2_main src/main.cpp)
target_link_libraries(project2_main project2_lib)

add_executable(naive-one src/naive-one.cpp)
target_link_libraries(naive-one project2_lib)

add_executable(naive-sender src/naive-sender.cpp)
target_link_libraries(naive-sender project2_lib)

add_executable(naive-receiver src/naive-receiver.cpp)
target_link_libraries(naive-receiver project2_lib)
