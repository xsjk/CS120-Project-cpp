
add_library(utils 
    STATIC 
        src/8b10b.cpp
)

target_include_directories(utils
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(utils
    PUBLIC
        Boost
)