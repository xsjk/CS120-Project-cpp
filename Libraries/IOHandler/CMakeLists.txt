
add_library(IOHandler INTERFACE)

target_include_directories(IOHandler
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(IOHandler INTERFACE)