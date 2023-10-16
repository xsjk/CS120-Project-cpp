
add_library(WASAPI INTERFACE)

target_include_directories(WASAPI
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(WASAPI INTERFACE Avrt IOHandler)
