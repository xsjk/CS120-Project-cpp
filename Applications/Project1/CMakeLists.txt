add_executable(project1_main main.cpp)
target_link_libraries(project1_main
    PUBLIC
        WASAPI
        ASIO
        Signal
)

add_executable(project1_task2 task2.cpp)
target_link_libraries(project1_task2 PRIVATE WASAPI ASIO)

add_executable(task3_matlab task3_matlab.cpp)
target_link_libraries(task3_matlab
    PRIVATE
        ASIO
        Signal
)