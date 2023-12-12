add_library(project3_lib INTERFACE)

target_include_directories(project3_lib
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(project3_lib
    INTERFACE
        Signal
        Asyncio
        argparse
        npcap
        project2_lib
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

add_executable(ping src/ping.cpp)
target_link_libraries(ping
    PRIVATE
        project3_lib
)
