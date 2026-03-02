cmake_minimum_required(VERSION 3.22)

project(VINYLwerk VERSION 1.0.0)

# Add JUCE (assumes it will be added as a submodule here just like the VST)
add_subdirectory(JUCE)

set(CMAKE_CXX_STANDARD 17)

# CLI Executable
juce_add_console_app(vinylwerk_cli
    PRODUCT_NAME "VINYLwerk CLI"
    VERSION "1.0.0"
)

target_sources(vinylwerk_cli PRIVATE
    Source/Main.cpp
    Source/DSP/ClickRemoval.cpp
    Source/DSP/Decrackle.cpp
    Source/DSP/NoiseReduction.cpp
    Source/DSP/FilterBank.cpp
    Source/DSP/SpectralProcessor.cpp
    Source/Utils/AudioFileManager.cpp
)

target_compile_definitions(vinylwerk_cli PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(vinylwerk_cli PRIVATE
    juce::juce_audio_basics
    juce::juce_audio_formats
    juce::juce_core
    juce::juce_dsp
    juce::juce_events
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)
