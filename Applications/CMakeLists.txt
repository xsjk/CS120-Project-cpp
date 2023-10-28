project(Project0)
add_subdirectory(Project0)

project(Project1)
add_subdirectory(Project1)

project(Example)
add_subdirectory(ASIOExample)
add_subdirectory(ASIOAudioExample)
add_subdirectory(UARTExample)
add_subdirectory(WASAPIExample)

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test)
    add_subdirectory(test)
endif()
