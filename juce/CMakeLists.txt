cmake_minimum_required(VERSION 3.22)
project(Acid303 VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── JUCE ───────────────────────────────────────────────────────
# Fetch JUCE 8 from GitHub if not already available
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.4
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

# ── Plugin target ──────────────────────────────────────────────
juce_add_plugin(Acid303
    COMPANY_NAME            "ACID-303"
    IS_SYNTH                TRUE
    NEEDS_MIDI_INPUT        TRUE
    NEEDS_MIDI_OUTPUT       FALSE
    IS_MIDI_EFFECT          FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE Ac3d
    PLUGIN_CODE              A303
    FORMATS                  AU VST3 Standalone
    PRODUCT_NAME             "ACID-303"
    BUNDLE_ID                "com.acid303.synth"
    AU_MAIN_TYPE             kAudioUnitType_MusicDevice
    VST3_CATEGORIES          "Instrument|Synth"
)

# ── Source files ───────────────────────────────────────────────
target_sources(Acid303
    PRIVATE
        Source/SynthSpec.h
        Source/DiodeLadderFilter.h
        Source/DiodeLadderFilter.cpp
        Source/Oscillator.h
        Source/Oscillator.cpp
        Source/Envelope.h
        Source/Envelope.cpp
        Source/LFO.h
        Source/LFO.cpp
        Source/Distortion.h
        Source/Distortion.cpp
        Source/TapeDelay.h
        Source/TapeDelay.cpp
        Source/Reverb.h
        Source/Reverb.cpp
        Source/Sequencer.h
        Source/Sequencer.cpp
        Source/AcidSynthEngine.h
        Source/AcidSynthEngine.cpp
        Source/PluginProcessor.h
        Source/PluginProcessor.cpp
        Source/PluginEditor.h
        Source/PluginEditor.cpp
)

target_compile_definitions(Acid303
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(Acid303
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
