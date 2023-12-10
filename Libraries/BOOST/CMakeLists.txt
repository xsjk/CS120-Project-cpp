cmake_minimum_required(VERSION 3.15)

find_package(Boost COMPONENTS json REQUIRED)
message(STATUS "Boost_VERSION: ${Boost_VERSION}")
message(STATUS "Boost_INCLUDE_DIRS: ${Boost_INCLUDE_DIRS}")

add_library(Boost INTERFACE)
target_link_libraries(Boost
    INTERFACE
        Boost::boost
        Boost::json
        # Wininet.lib
        Ws2_32.lib
        # winmm.lib
        # shlwapi.lib
        # version.lib
        # imm32.lib
        # dxgi.lib
        Mswsock.lib
)
