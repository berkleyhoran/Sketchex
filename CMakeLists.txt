cmake_minimum_required(VERSION 3.22)

# Overridable from the command line (`-DSKETCHEX_VERSION=0.2.0`) so CI can
# drive the plugin/installer version straight from a git tag without
# editing this file -- see .github/workflows/release.yml. Local/manual
# builds that don't pass -D get the latest git tag instead (falling back
# to 0.1.0 if git/tags aren't available). -D still wins: set(... CACHE)
# never overwrites an already-set cache entry. Same scheme as Reactex and
# Synthex, gexex's sibling plugins.
set(_sketchex_version_default "0.1.0")
find_package(Git QUIET)
if(GIT_FOUND)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} describe --tags --abbrev=0
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        OUTPUT_VARIABLE _sketchex_git_tag
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        RESULT_VARIABLE _sketchex_git_result)
    if(_sketchex_git_result EQUAL 0 AND _sketchex_git_tag)
        string(REGEX REPLACE "^v" "" _sketchex_version_default "${_sketchex_git_tag}")
    endif()
endif()
set(SKETCHEX_VERSION "${_sketchex_version_default}" CACHE STRING "Sketchex version (plugin + installers)")

project(Sketchex VERSION ${SKETCHEX_VERSION})

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(SKETCHEX_BUILD_TESTS "Build the JUCE-free core unit tests" ON)

# ---------------------------------------------------------------------------
# Core (pure C++, no JUCE): scale quantisation, the sketch model, and the
# sequencer engine that turns strokes + a playhead into MIDI events. Kept
# JUCE-free so it can be unit-tested in milliseconds without a plugin host.
# ---------------------------------------------------------------------------
add_library(SketchexCore STATIC
    Source/Core/Scale.cpp
    Source/Core/Sketch.cpp
    Source/Core/SequencerEngine.cpp)
target_include_directories(SketchexCore PUBLIC Source)
set_target_properties(SketchexCore PROPERTIES POSITION_INDEPENDENT_CODE ON)

if(SKETCHEX_BUILD_TESTS)
    enable_testing()
    add_executable(SketchexCoreTests Tests/CoreTests.cpp)
    target_link_libraries(SketchexCoreTests PRIVATE SketchexCore)
    add_test(NAME SketchexCoreTests COMMAND SketchexCoreTests)
endif()

# ---------------------------------------------------------------------------
# JUCE
# ---------------------------------------------------------------------------
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.15
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(JUCE)

# ---------------------------------------------------------------------------
# Plugin target
# ---------------------------------------------------------------------------
set(SKETCHEX_LOGO "${CMAKE_CURRENT_SOURCE_DIR}/Assets/logo.png")
set(SKETCHEX_ICON_ARGS "")
if(EXISTS ${SKETCHEX_LOGO})
    set(SKETCHEX_ICON_ARGS ICON_BIG "${SKETCHEX_LOGO}" ICON_SMALL "${SKETCHEX_LOGO}")
endif()

# AU only makes sense (and only builds) on Apple. Logic Pro in particular
# only loads MIDI-generating plugins as AU "MIDI FX" (aumi), which is
# exactly what IS_MIDI_EFFECT TRUE produces on that platform.
set(SKETCHEX_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND SKETCHEX_FORMATS AU)
endif()

juce_add_plugin(Sketchex
    ${SKETCHEX_ICON_ARGS}
    COMPANY_NAME "gexex"
    BUNDLE_ID "com.gexex.sketchex"
    PLUGIN_MANUFACTURER_CODE Gxex
    PLUGIN_CODE Skx1
    FORMATS ${SKETCHEX_FORMATS}
    PRODUCT_NAME "Sketchex"
    # A MIDI effect: no audio in/out of its own, MIDI in (passed through)
    # and MIDI out (the drawn notes). Hosts that don't support VST3 MIDI
    # effects (some route them as instruments) still see a MIDI-out
    # plugin they can wire to a synth on another track.
    VST3_CATEGORIES "Instrument" "Tools"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT TRUE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE)

juce_generate_juce_header(Sketchex)

target_sources(Sketchex PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Parameters.cpp
    Source/UpdateChecker.cpp
    Source/GUI/LookAndFeel.cpp
    Source/GUI/SketchCanvas.cpp
    Source/GUI/ControlStrip.cpp)

target_compile_definitions(Sketchex PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    SKETCHEX_VERSION_STRING="${SKETCHEX_VERSION}")

target_link_libraries(Sketchex PRIVATE
    SketchexCore
    juce::juce_audio_utils
    juce::juce_dsp
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)

# Headless end-to-end smoke test: the real processor driven by a fake
# host playhead (see Tests/ProcessorSmoke.cpp). Linked against the
# plugin's shared-code target so it uses the exact same objects.
if(SKETCHEX_BUILD_TESTS)
    add_executable(SketchexProcessorSmoke Tests/ProcessorSmoke.cpp)
    # The plugin's own module include paths / defines are PRIVATE on the
    # Sketchex target, so pull the same ones in explicitly.
    target_link_libraries(SketchexProcessorSmoke PRIVATE
        Sketchex
        juce::juce_audio_plugin_client
        juce::juce_audio_utils
        juce::juce_dsp)
    target_include_directories(SketchexProcessorSmoke PRIVATE Source
        "${CMAKE_CURRENT_BINARY_DIR}/Sketchex_artefacts/JuceLibraryCode")
    target_compile_definitions(SketchexProcessorSmoke PRIVATE
        $<TARGET_PROPERTY:Sketchex,COMPILE_DEFINITIONS>)
    add_test(NAME SketchexProcessorSmoke COMMAND SketchexProcessorSmoke)
endif()
