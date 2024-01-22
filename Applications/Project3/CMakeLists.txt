add_library(project3_lib INTERFACE)

target_include_directories(project3_lib
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(project3_lib
    INTERFACE
        argparse
        npcap
        wintun
        utils
        aethernet
)

add_executable(project3_main src/main.cpp)
target_link_libraries(project3_main project3_lib)

add_executable(asyncio_test src/asyncio_test.cpp)
target_link_libraries(asyncio_test project3_lib)

add_executable(project3_ping src/ping.cpp)
target_link_libraries(project3_ping project3_lib)

add_executable(project3_node1 src/node1.cpp)
target_link_libraries(project3_node1 project3_lib)

add_executable(project3_node2 src/node2.cpp)
target_link_libraries(project3_node2 project3_lib)

add_executable(project3_nodes src/nodes.cpp)
target_link_libraries(project3_nodes project3_lib)

add_executable(project3_aethernet src/aethernet.cpp)
target_link_libraries(project3_aethernet project3_lib)

add_executable(project3_forward_naive src/forward_naive.cpp)
target_link_libraries(project3_forward_naive project3_lib)

add_executable(project3_forward_naive2 src/forward_naive2.cpp)
target_link_libraries(project3_forward_naive2 project3_lib)

add_executable(project3_forward_naive3 src/forward_naive3.cpp)
target_link_libraries(project3_forward_naive3 project3_lib)
