cmake_minimum_required(VERSION 3.22)
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" CACHE STRING "")

project(FillMaker VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT FILLMAKER_JUCE_PATH)
    if(DEFINED ENV{JUCE_PATH})
        set(FILLMAKER_JUCE_PATH "$ENV{JUCE_PATH}")
    elseif(WIN32)
        set(FILLMAKER_JUCE_PATH "C:/JUCE")
    else()
        set(FILLMAKER_JUCE_PATH "$ENV{HOME}/JUCE")
    endif()
endif()
add_subdirectory("${FILLMAKER_JUCE_PATH}" "${CMAKE_BINARY_DIR}/JUCE" EXCLUDE_FROM_ALL)

set(FILLMAKER_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND FILLMAKER_FORMATS AU)
endif()

# The generator: pure C++ on juce_core, no plug-in (tests and the command-line tool use it directly).
add_library(fillmaker_engine STATIC
    source/engine/Pattern.cpp
    source/engine/Settings.cpp
    source/engine/Rhythm.cpp
    source/engine/Grammar.cpp
    source/engine/Pitch.cpp
    source/engine/Engines.cpp
    source/engine/Generator.cpp
    source/engine/Mutation.cpp
    source/engine/Groove.cpp
    source/engine/MidiExport.cpp
    source/engine/Presets.cpp
    source/playback/Player.cpp
    source/playback/PreviewSynth.cpp)
target_include_directories(fillmaker_engine PUBLIC source)
target_link_libraries(fillmaker_engine PUBLIC juce::juce_core juce::juce_audio_basics)
target_compile_definitions(fillmaker_engine PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_MODAL_LOOPS_PERMITTED=1)

option(FILLMAKER_ENGINE_ONLY "Only the engine library (no plug-in, tools or tests)" OFF)
if(FILLMAKER_ENGINE_ONLY)
    # The engine's own tests (generation, playback, export), without the plug-in.
    include(FetchContent)
    FetchContent_Declare(Catch2 GIT_REPOSITORY https://github.com/catchorg/Catch2.git GIT_TAG v3.7.1 GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(Catch2)
    juce_add_console_app(FillMakerEngineTests PRODUCT_NAME "fillmaker_engine_tests")
    target_sources(FillMakerEngineTests PRIVATE tests/test_main.cpp tests/engine_tests.cpp tests/playback_tests.cpp)
    target_compile_definitions(FillMakerEngineTests PRIVATE JUCE_STANDALONE_APPLICATION=1)
    target_link_libraries(FillMakerEngineTests PRIVATE fillmaker_engine juce::juce_events Catch2::Catch2 juce::juce_recommended_config_flags)
    return()
endif()

# An instrument with MIDI in and out: it sits on a MIDI track in Live, hears that track's clip (the
# groove), and its MIDI goes to the drum track ("MIDI From: <this track>, Fill Maker"). Its audio
# output is only the preview voice.
juce_add_plugin(FillMaker
    COMPANY_NAME            "LeverStudios"
    PLUGIN_MANUFACTURER_CODE Lvrs
    PLUGIN_CODE             Fmkr
    BUNDLE_ID               "com.leverstudios.fillmaker"
    PRODUCT_NAME            "Fill Maker"
    FORMATS                 ${FILLMAKER_FORMATS}
    IS_SYNTH                TRUE
    NEEDS_MIDI_INPUT        TRUE
    NEEDS_MIDI_OUTPUT       TRUE
    IS_MIDI_EFFECT          FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES         Instrument Drum Generator
    AU_MAIN_TYPE            kAudioUnitType_MusicDevice)

add_library(fillmaker_plugin_code INTERFACE)
target_sources(fillmaker_plugin_code INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/source/plugin/PluginProcessor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/source/plugin/PluginEditor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/source/plugin/PatternView.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/source/plugin/Look.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/source/plugin/Panels.cpp)
target_include_directories(fillmaker_plugin_code INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/source)
target_link_libraries(fillmaker_plugin_code INTERFACE fillmaker_engine juce::juce_audio_utils juce::juce_audio_formats)

target_link_libraries(FillMaker PRIVATE fillmaker_plugin_code PUBLIC
    juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

# fillmaker-cli: generate fills and write .mid files from the command line; also snapshots the editor.
juce_add_console_app(FillMakerCli PRODUCT_NAME "fillmaker-cli")
target_sources(FillMakerCli PRIVATE tools/cli.cpp)
target_compile_definitions(FillMakerCli PRIVATE JucePlugin_Name="Fill Maker" JUCE_STANDALONE_APPLICATION=1)
target_link_libraries(FillMakerCli PRIVATE fillmaker_plugin_code juce::juce_recommended_config_flags)

option(FILLMAKER_BUILD_TESTS "Build tests" ON)
if(FILLMAKER_BUILD_TESTS)
    include(FetchContent)
    FetchContent_Declare(Catch2 GIT_REPOSITORY https://github.com/catchorg/Catch2.git GIT_TAG v3.7.1 GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(Catch2)
    enable_testing()
    juce_add_console_app(FillMakerTests PRODUCT_NAME "fillmaker_tests")
    target_sources(FillMakerTests PRIVATE tests/test_main.cpp tests/engine_tests.cpp tests/playback_tests.cpp tests/plugin_tests.cpp)
    target_compile_definitions(FillMakerTests PRIVATE JucePlugin_Name="Fill Maker" JUCE_STANDALONE_APPLICATION=1)
    target_link_libraries(FillMakerTests PRIVATE fillmaker_plugin_code Catch2::Catch2 juce::juce_recommended_config_flags)
    add_test(NAME fillmaker_tests COMMAND FillMakerTests)
endif()
