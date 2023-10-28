add_executable(project1_main main.cpp)
target_link_libraries(project1_main 
    PUBLIC
        WASAPI
        Signal
)
