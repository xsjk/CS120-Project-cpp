add_library(PhysicalLayer INTERFACE)

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


add_executable(project1_main src/main.cpp)
target_link_libraries(project1_main
    PRIVATE
        PhysicalLayer
)

add_executable(project1_task2 src/task2.cpp)
target_link_libraries(project1_task2 
    PRIVATE
        PhysicalLayer
)

add_executable(task3_matlab src/task3_matlab.cpp)
target_link_libraries(task3_matlab
    PRIVATE
        PhysicalLayer
)