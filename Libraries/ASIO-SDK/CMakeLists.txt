add_library(ASIO INTERFACE)

target_include_directories(ASIO INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/common)

target_compile_definitions(ASIO 
    INTERFACE
        JUCE_USE_WINRT_MIDI=0
        JUCE_ASIO=1
        JUCE_WASAPI=0
        JUCE_DIRECTSOUND=0
        JUCE_ALSA=0
        JUCE_JACK=0
        JUCE_BELA=0
    )