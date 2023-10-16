file(GLOB SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)

add_library(WAVE SHARED ${SOURCES})

target_include_directories(WAVE 
    PUBLIC 
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
