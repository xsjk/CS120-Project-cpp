add_library(wintun INTERFACE)
target_include_directories(wintun INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(wintun 
    INTERFACE 
        Asyncio
        Ntdll 
        Ws2_32 
        IPHLPAPI
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/amd64/libwintun.a
)

# copy the wintun.dll to the build directory
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/lib/amd64/wintun.dll ${CMAKE_RUNTIME_OUTPUT_DIRECTORY} COPYONLY)

add_executable(WintunExample ${CMAKE_CURRENT_SOURCE_DIR}/src/example.c)
target_link_libraries(WintunExample wintun)

