add_executable(JUCEExample Main.cpp)

target_link_libraries(JUCEExample
    PRIVATE
        ASIO_JUCE
)