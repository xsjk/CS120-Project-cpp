juce_add_console_app(PlayerExample PRODUCT_NAME "PlayerExample")
juce_generate_juce_header(PlayerExample)

target_sources(PlayerExample
    PRIVATE
        Main.cpp)

target_link_libraries(PlayerExample
    PRIVATE
        ASIO
        juce::juce_core
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
