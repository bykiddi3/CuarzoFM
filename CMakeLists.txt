cmake_minimum_required(VERSION 3.22)

project(CuarzoFM VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(CUARZO_BUILD_STANDALONE "Compilar tambien la aplicacion Standalone" ON)

# ------------------------------------------------------------------------------
# JUCE: usa una copia local (-DJUCE_DIR=/ruta/a/JUCE) o la descarga automaticamente
# ------------------------------------------------------------------------------
if(DEFINED JUCE_DIR)
    add_subdirectory(${JUCE_DIR} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.9
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

set(CUARZO_FORMATS VST3)
if(CUARZO_BUILD_STANDALONE)
    list(APPEND CUARZO_FORMATS Standalone)
endif()

juce_add_plugin(CuarzoFM
    COMPANY_NAME                "Cuarzo Audio"
    COMPANY_WEBSITE             "https://example.com"
    PLUGIN_MANUFACTURER_CODE    Cuaz
    PLUGIN_CODE                 Cfm4
    FORMATS                     ${CUARZO_FORMATS}
    PRODUCT_NAME                "Cuarzo FM"
    IS_SYNTH                    TRUE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    VST3_CATEGORIES             Instrument Synth)

target_sources(CuarzoFM PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/SynthVoice.cpp
    Source/Presets.cpp)

target_include_directories(CuarzoFM PRIVATE Source)

target_compile_definitions(CuarzoFM PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

set(CUARZO_JUCE_MODULES
    juce::juce_audio_basics
    juce::juce_audio_processors
    juce::juce_gui_basics)
if(CUARZO_BUILD_STANDALONE)
    list(APPEND CUARZO_JUCE_MODULES juce::juce_audio_utils)
endif()

target_link_libraries(CuarzoFM
    PRIVATE
        ${CUARZO_JUCE_MODULES}
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
