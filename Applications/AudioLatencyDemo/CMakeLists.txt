juce_add_gui_app(LatencyTestGUI PRODUCT_NAME "LatencyTest")

juce_generate_juce_header(LatencyTestGUI)

target_sources(LatencyTestGUI PRIVATE Source/Main.cpp)

target_link_libraries(LatencyTestGUI
    PRIVATE
        ASIO # USE ASIO as audio backend
        juce::juce_gui_extra
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

