
add_library(Utils 
    STATIC 
        src/8b10b.cpp
)

target_include_directories(Utils
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(Utils
    PUBLIC
        Boost
)