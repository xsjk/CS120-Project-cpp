add_library(project3_lib INTERFACE)

target_include_directories(project3_lib
    INTERFACE
        include
)

target_link_libraries(project3_lib
    INTERFACE
        Boost
        Utils
)

add_executable(project3_main src/main.cpp)
target_link_libraries(project3_main
    PRIVATE
        project3_lib
)

add_executable(asyncio_test src/asyncio_test.cpp)
target_link_libraries(asyncio_test
    PRIVATE
        project3_lib
)
