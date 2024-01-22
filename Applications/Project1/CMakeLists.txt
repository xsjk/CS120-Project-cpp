add_library(project1_lib INTERFACE)

target_include_directories(project1_lib
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(project1_lib
    INTERFACE
        WASAPI
        ASIO
)

add_executable(project1_main src/main.cpp)
target_link_libraries(project1_main project1_lib)

add_executable(project1_task2 src/task2.cpp)
target_link_libraries(project1_task2 project1_lib)

add_executable(task3_matlab src/task3_matlab.cpp)
target_link_libraries(task3_matlab project1_lib)
