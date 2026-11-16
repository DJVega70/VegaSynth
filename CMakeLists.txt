cmake_minimum_required(VERSION 3.22)
project(PcmWorkstation VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.9
    GIT_SHALLOW ON)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(PcmWorkstation
    COMPANY_NAME "StudioTools"
    PLUGIN_MANUFACTURER_CODE Stlz
    PLUGIN_CODE Pcm1
    FORMATS VST3 Standalone
    PRODUCT_NAME "PCM Workstation"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    VST3_CATEGORIES Instrument Synth
    COPY_PLUGIN_AFTER_BUILD FALSE)

juce_generate_juce_header(PcmWorkstation)

target_sources(PcmWorkstation PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(PcmWorkstation PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(PcmWorkstation
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags)

# DSP unit tests (plain C++, no JUCE):  g++ -O2 -std=c++17 tests/pcm_test.cpp -o pcm_test

# Optional smoke test of the real processor (MIDI in, combination loading, state)
add_executable(processor_smoke EXCLUDE_FROM_ALL
    tests/processor_smoke.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp)
target_include_directories(processor_smoke PRIVATE
    ${CMAKE_BINARY_DIR}/PcmWorkstation_artefacts/JuceLibraryCode ${juce_SOURCE_DIR}/modules)
target_compile_definitions(processor_smoke PRIVATE
    $<TARGET_PROPERTY:PcmWorkstation,COMPILE_DEFINITIONS> JUCE_STANDALONE_APPLICATION=0 JUCE_MODAL_LOOPS_PERMITTED=1)
target_link_libraries(processor_smoke PRIVATE juce::juce_audio_utils juce::juce_recommended_config_flags)

# Optional: load the built VST3 through JUCE's plugin host (what a DAW does)
add_executable(host_load EXCLUDE_FROM_ALL tests/host_load.cpp)
target_include_directories(host_load PRIVATE ${CMAKE_BINARY_DIR}/PcmWorkstation_artefacts/JuceLibraryCode ${juce_SOURCE_DIR}/modules)
target_compile_definitions(host_load PRIVATE JUCE_PLUGINHOST_VST3=1 JUCE_STANDALONE_APPLICATION=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_MODAL_LOOPS_PERMITTED=1)
target_link_libraries(host_load PRIVATE juce::juce_audio_utils juce::juce_recommended_config_flags)

# Optional: render the editor pages to PNG (used for the README screenshots)
add_executable(editor_shot EXCLUDE_FROM_ALL tests/editor_shot.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp)
target_include_directories(editor_shot PRIVATE ${CMAKE_BINARY_DIR}/PcmWorkstation_artefacts/JuceLibraryCode ${juce_SOURCE_DIR}/modules)
target_compile_definitions(editor_shot PRIVATE $<TARGET_PROPERTY:PcmWorkstation,COMPILE_DEFINITIONS> JUCE_STANDALONE_APPLICATION=0 JUCE_MODAL_LOOPS_PERMITTED=1)
target_link_libraries(editor_shot PRIVATE juce::juce_audio_utils juce::juce_recommended_config_flags)
