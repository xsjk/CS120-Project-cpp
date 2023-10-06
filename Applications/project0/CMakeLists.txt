juce_add_console_app(project0_ver_A PRODUCT_NAME "project0")
juce_generate_juce_header(project0_ver_A)

target_sources(project0_ver_A
    PRIVATE
        main_ver_A.cpp)

target_link_libraries(project0_ver_A
    PRIVATE
        ASIO
        juce::juce_core
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)



juce_add_console_app(project0_ver_B PRODUCT_NAME "project0")
juce_generate_juce_header(project0_ver_B)

target_sources(project0_ver_B
    PRIVATE
        main_ver_B.cpp)

target_link_libraries(project0_ver_B
    PRIVATE
        ASIO
        juce::juce_core
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)