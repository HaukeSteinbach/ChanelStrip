cmake_minimum_required(VERSION 3.22)

project(SteinbachChanelStrip VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)   # für VS Code IntelliSense

# JUCE als Submodul
add_subdirectory(JUCE)

juce_add_plugin(SteinbachChanelStrip
    COMPANY_NAME             "Steinbach Audio"
    COMPANY_WEBSITE          ""
    COMPANY_EMAIL            ""

    IS_SYNTH                 FALSE
    NEEDS_MIDI_INPUT         FALSE
    NEEDS_MIDI_OUTPUT        FALSE
    IS_MIDI_EFFECT           FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE

    COPY_PLUGIN_AFTER_BUILD  TRUE

    PLUGIN_MANUFACTURER_CODE Stbh
    PLUGIN_CODE              Stcs

    FORMATS                  VST3 AU Standalone

    PRODUCT_NAME             "Steinbach Chanel Strip"
    BUNDLE_ID                "com.steinbach-audio.chanel-strip"

    VST3_CATEGORIES          "Fx" "EQ" "Dynamics"
)

# ── Quellen ──────────────────────────────────────────────────────────────────
target_sources(SteinbachChanelStrip
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/DSP/EQProcessor.cpp
        Source/DSP/WavetableManager.cpp
        Source/ConsoleMode/InstanceRegistry.cpp
        Source/SharedMemory/SharedParameterBlock.cpp
)

target_include_directories(SteinbachChanelStrip
    PRIVATE
        Source
)

# ── Compile-Definitionen ──────────────────────────────────────────────────────
target_compile_definitions(SteinbachChanelStrip
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
)

# ── Abhängigkeiten ────────────────────────────────────────────────────────────
target_link_libraries(SteinbachChanelStrip
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# JuceHeader.h generieren (enthält alle konfigurierten Module)
juce_generate_juce_header(SteinbachChanelStrip)

# ── SteinbachConsole ──────────────────────────────────────────────────────────
juce_add_plugin(SteinbachConsole
    COMPANY_NAME             "Steinbach Audio"
    IS_SYNTH                 FALSE
    NEEDS_MIDI_INPUT         FALSE
    NEEDS_MIDI_OUTPUT        FALSE
    IS_MIDI_EFFECT           FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD  TRUE
    PLUGIN_MANUFACTURER_CODE Stbh
    PLUGIN_CODE              Stco
    FORMATS                  VST3 Standalone
    PRODUCT_NAME             "Steinbach Console"
    BUNDLE_ID                "com.steinbach-audio.console"
    VST3_CATEGORIES          "Fx" "Tools"
)

target_sources(SteinbachConsole
    PRIVATE
        Source/Console/ConsoleProcessor.cpp
        Source/Console/ConsoleEditor.cpp
        Source/SharedMemory/SharedParameterBlock.cpp
)

target_include_directories(SteinbachConsole
    PRIVATE
        Source
)

target_compile_definitions(SteinbachConsole
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(SteinbachConsole
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

juce_generate_juce_header(SteinbachConsole)
