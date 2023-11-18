project(Project0)
add_subdirectory(Project0)

project(Project1)
add_subdirectory(Project1)

project(Project2)
add_subdirectory(Project2)

project(Example VERSION 0.0.1)
add_subdirectory(ASIOExample)
add_subdirectory(ASIOAudioExample)
add_subdirectory(UARTExample)
add_subdirectory(WASAPIExample)
add_subdirectory(JUCEExample)
add_subdirectory(AudioLatencyDemo)

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test)
    add_subdirectory(test)
endif()
