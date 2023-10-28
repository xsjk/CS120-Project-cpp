add_executable(project1_main main.cpp)
target_link_libraries(project1_main
    PUBLIC
        WASAPI
        Signal
)

add_executable(project1_task2 task2.cpp)
target_link_libraries(project1_task2 PRIVATE WASAPI ASIO)