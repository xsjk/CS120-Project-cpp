file(GLOB COMMON_SOURCES "common/*.cpp")
file(GLOB HOST_SOURCES "host/*.cpp")
file(GLOB HOST_PC_SOURCES "host/pc/*.cpp")
list(REMOVE_ITEM COMMON_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/common/asiodrvr.cpp")
list(REMOVE_ITEM COMMON_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/common/dllentry.cpp")

add_library(ASIO SHARED ${COMMON_SOURCES} ${HOST_SOURCES} ${HOST_PC_SOURCES})

# for clang
find_package(Threads REQUIRED)
target_link_libraries(ASIO ${CMAKE_THREAD_LIBS_INIT} utils)
target_include_directories(ASIO PUBLIC common host host/pc)
target_compile_definitions(ASIO PRIVATE ASIO_EXPORTS)

