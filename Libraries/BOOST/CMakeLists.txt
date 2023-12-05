cmake_minimum_required(VERSION 3.15)

find_package(Boost REQUIRED)
include_directories(${Boost_INCLUDE_DIRS})
message(STATUS "Boost_VERSION: ${Boost_VERSION}")
message(STATUS "Boost_INCLUDE_DIRS: ${Boost_INCLUDE_DIRS}")

add_library(Boost INTERFACE)
target_include_directories(Boost INTERFACE ${Boost_INCLUDE_DIRS})
target_link_libraries(Boost
    INTERFACE
        # Wininet.lib
        Ws2_32.lib
        # winmm.lib
        # shlwapi.lib
        # version.lib
        # imm32.lib
        # dxgi.lib
        Mswsock.lib
)
