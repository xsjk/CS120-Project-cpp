
set(JUCE_MODULE ${CMAKE_CURRENT_SOURCE_DIR}/../JUCE/modules)

add_library(ASIO_JUCE 
    SHARED
        ${JUCE_MODULE}/juce_audio_basics/juce_audio_basics.cpp
        ${JUCE_MODULE}/juce_audio_devices/juce_audio_devices.cpp
        ${JUCE_MODULE}/juce_audio_formats/juce_audio_formats.cpp
        ${JUCE_MODULE}/juce_audio_processors/juce_audio_processors.cpp
        ${JUCE_MODULE}/juce_audio_processors/juce_audio_processors_ara.cpp
        ${JUCE_MODULE}/juce_audio_processors/juce_audio_processors_lv2_libs.cpp
        ${JUCE_MODULE}/juce_audio_utils/juce_audio_utils.cpp
        ${JUCE_MODULE}/juce_data_structures/juce_data_structures.cpp
        ${JUCE_MODULE}/juce_events/juce_events.cpp
        ${JUCE_MODULE}/juce_graphics/juce_graphics.cpp
        ${JUCE_MODULE}/juce_gui_basics/juce_gui_basics.cpp
        ${JUCE_MODULE}/juce_gui_extra/juce_gui_extra.cpp
        ${JUCE_MODULE}/juce_core/juce_core.cpp
        src/jucedevice.cpp
)

target_include_directories(ASIO_JUCE
    PUBLIC
        ${JUCE_MODULE}
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(ASIO_JUCE
    PUBLIC
        ASIO
        Utils
)

target_compile_definitions(ASIO_JUCE
    PUBLIC
        JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1
        JUCE_USE_WINRT_MIDI=0
        JUCE_ASIO=1
        JUCE_WASAPI=0
        JUCE_DIRECTSOUND=0
        JUCE_ALSA=0
        JUCE_JACK=0
        JUCE_BELA=0
)

target_link_libraries(ASIO_JUCE
    PUBLIC
        Wininet.lib
        Ws2_32.lib
        winmm.lib
        shlwapi.lib
        version.lib
        imm32.lib
        dxgi.lib
)
