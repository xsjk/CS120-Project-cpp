file(GLOB Aethernet_SOURCES src/*.cpp)

add_library(aethernet SHARED ${Aethernet_SOURCES})

target_compile_definitions(aethernet PRIVATE AETHERNET_EXPORTS)

target_include_directories(aethernet 
    PUBLIC 
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(aethernet 
    PUBLIC
        Asyncio
        ASIO
)
