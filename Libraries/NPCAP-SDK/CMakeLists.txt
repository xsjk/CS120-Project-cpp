add_library(npcap INTERFACE)
target_include_directories(npcap INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/Include)

if (${CMAKE_SYSTEM_PROCESSOR} STREQUAL "AMD64")
    target_link_directories(npcap INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/Lib/x64)
elseif (${CMAKE_SYSTEM_PROCESSOR} STREQUAL "x86")
    target_link_directories(npcap INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/Lib/x86)
elseif (${CMAKE_SYSTEM_PROCESSOR} STREQUAL "ARM64")
    target_link_directories(npcap INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/Lib/ARM64)
else()
    message(FATAL_ERROR "Unsupported architecture")
endif()

target_link_libraries(npcap INTERFACE Packet wpcap ws2_32)


function(add_npcap_examples_in_directory DIR)
    # Find all .c and .cpp files in the given directory
    file(GLOB_RECURSE SOURCES RELATIVE ${DIR} "${DIR}/*.c" "${DIR}/*.cpp")

    get_filename_component(BASEDIR ${DIR} NAME)

    message(STATUS "Processing ${BASEDIR}...")

    foreach(SOURCE ${SOURCES})

        # Get the file dirname
        get_filename_component(TARGET_DIR ${SOURCE} DIRECTORY)

        # Replace slashes with underscores to create a unique target name
        string(REPLACE "/" "-" TARGET_NAME ${SOURCE})

        # Get the file name without the extension (as target name)
        get_filename_component(TARGET_NAME ${TARGET_NAME} NAME_WE)

        # Add the executable target 
        add_executable(${BASEDIR}-${TARGET_NAME} ${DIR}/${SOURCE})
        target_link_libraries(${BASEDIR}-${TARGET_NAME} npcap)
        
    endforeach()
endfunction()

add_npcap_examples_in_directory(${CMAKE_CURRENT_SOURCE_DIR}/Examples-pcap)
add_npcap_examples_in_directory(${CMAKE_CURRENT_SOURCE_DIR}/Examples-remote)

