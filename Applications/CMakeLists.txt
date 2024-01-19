project(Project0)
add_subdirectory(Project0)

project(Project1)
add_subdirectory(Project1)

project(Project2)
add_subdirectory(Project2)

project(Project3)
add_subdirectory(Project3)

project(Project4)
add_subdirectory(Project4)

project(Example)
add_subdirectory(ASIOExample)
add_subdirectory(ASIOAudioExample)
add_subdirectory(UARTExample)
add_subdirectory(WASAPIExample)

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test)
    add_subdirectory(test)
endif()
