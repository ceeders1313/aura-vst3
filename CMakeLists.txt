cmake_minimum_required(VERSION 3.22)

# Runtime statique : la DLL n'exige pas le redistribuable Visual C++
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

project(AURA VERSION 0.10.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW ON)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(AURA
    COMPANY_NAME "AuraAudio"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    PLUGIN_MANUFACTURER_CODE Aura
    PLUGIN_CODE Aur1
    FORMATS VST3
    VST3_CATEGORIES Instrument Synth
    PRODUCT_NAME "AURA"
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(AURA PRIVATE
    src/PluginProcessor.cpp
    src/PluginEditor.cpp
    src/data/BankLoader.cpp
    src/data/PresetLibrary.cpp)

target_include_directories(AURA PRIVATE src)

target_compile_definitions(AURA PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(AURA
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Tests automatiques (rendu audio + captures UI), hors CI : -DAURA_BUILD_TESTS=ON
option(AURA_BUILD_TESTS "Build the AuraTests console app" OFF)
if(AURA_BUILD_TESTS)
    juce_add_console_app(AuraTests PRODUCT_NAME "AuraTests")
    target_sources(AuraTests PRIVATE
        tests/AuraTests.cpp
        src/PluginProcessor.cpp
        src/PluginEditor.cpp
        src/data/BankLoader.cpp
        src/data/PresetLibrary.cpp)
    target_include_directories(AuraTests PRIVATE src)
    target_compile_definitions(AuraTests PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_STANDALONE_APPLICATION=1
        JucePlugin_VersionString="${PROJECT_VERSION}")
    target_link_libraries(AuraTests PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
endif()
