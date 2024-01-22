add_library(project4_lib INTERFACE)

target_link_libraries(project4_lib 
    INTERFACE
        aethernet
        argparse
        wintun
)

add_executable(project4_aethernet_host src/aethernet.cpp)
target_compile_definitions(project4_aethernet_host PRIVATE HOST)
target_link_libraries(project4_aethernet_host project4_lib)

add_executable(project4_aethernet_router src/aethernet.cpp)
target_compile_definitions(project4_aethernet_router PRIVATE ROUTER)
target_link_libraries(project4_aethernet_router project4_lib)

add_executable(project4_aethernet_tcp_forward_host src/aethernet_tcp_forward.cpp)
target_compile_definitions(project4_aethernet_tcp_forward_host PRIVATE HOST)
target_link_libraries(project4_aethernet_tcp_forward_host project4_lib)

add_executable(project4_aethernet_tcp_forward_router src/aethernet_tcp_forward.cpp)
target_compile_definitions(project4_aethernet_tcp_forward_router PRIVATE ROUTER)
target_link_libraries(project4_aethernet_tcp_forward_router project4_lib)
