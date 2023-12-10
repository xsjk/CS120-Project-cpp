add_executable(project3_main src/main.cpp)
target_link_libraries(project3_main
    PRIVATE
        Asyncio
        PhysicalLayer
        argparse
)

add_executable(asyncio_test src/asyncio_test.cpp)
target_link_libraries(asyncio_test
    PRIVATE
        Asyncio
)
