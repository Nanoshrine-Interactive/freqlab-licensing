cmake_minimum_required(VERSION 3.22)

# Toolchain settings MUST precede project() so the licensing SDK (loaded
# via CMAKE_PROJECT_INCLUDE during project()) inherits the same CRT,
# arch list, and deployment target. Setting these after project() leads
# to LNK2038 on Windows or single-arch macOS binaries.
if(WIN32)
    # Static CRT - end-users don't need the VC++ redistributable.
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" CACHE INTERNAL "")
endif()

if(APPLE)
    # 10.15 is the SDK's floor (std::filesystem in libc++).
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum macOS version")
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Universal binary")
endif()

project(Jucerguy VERSION 1.0.0)

# Emit compile_commands.json so clangd (LSP) can resolve include paths
# without a custom configuration. Ignored by build; only used by editors.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Fetch JUCE
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.12
)

# Fetch clap-juce-extensions for CLAP format support (JUCE 8 doesn't have native CLAP)
FetchContent_Declare(
    clap-juce-extensions
    GIT_REPOSITORY https://github.com/free-audio/clap-juce-extensions.git
    GIT_TAG main
)

FetchContent_MakeAvailable(JUCE clap-juce-extensions)

# AAX SDK (optional - pass -DAAX_SDK_PATH=<path> to enable AAX builds)
if(DEFINED AAX_SDK_PATH)
    juce_set_aax_sdk_path("${AAX_SDK_PATH}")
endif()

# Plugin definition
juce_add_plugin(Jucerguy
    COMPANY_NAME "freqlab"
    PLUGIN_MANUFACTURER_CODE FREQ
    PLUGIN_CODE 811C
    LV2URI "urn:freqlab:jucerguy"

    # Formats: CLAP added via clap-juce-extensions below. AAX requires Avid SDK (skipped if not found).
    FORMATS VST3

    PRODUCT_NAME "Jucerguy"

    # Don't auto-install to system plugin dirs - freqlab handles publishing
    COPY_PLUGIN_AFTER_BUILD FALSE

    # Plugin characteristics (effect, not synth)
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE

    # VST3 categories
    VST3_CATEGORIES "Fx"
)

# Add CLAP format via clap-juce-extensions (required for freqlab preview)
clap_juce_extensions_plugin(
    TARGET Jucerguy
    CLAP_ID "com.freqlab.jucerguy"
    CLAP_FEATURES "audio-effect" "stereo"
)

# Source files
target_sources(Jucerguy
    PRIVATE
        src/PluginProcessor.cpp
        src/PluginEditor.cpp
)

# Include directories
target_include_directories(Jucerguy PRIVATE src)

# freqlab licensing. The cloud build injects the real SDK during
# project() via CMAKE_PROJECT_INCLUDE; the guard avoids duplicate-target
# there. Everywhere else we FetchContent the public stub (returns
# Status::NoConfig - UI stays hidden).
#
# To preview each license state locally, pass a dev flag on the CLI:
#   cmake -DFREQLAB_LICENSING_DEV_LICENSED=ON ...      # green
#   cmake -DFREQLAB_LICENSING_DEV_EXPIRED=ON ...       # red
#   cmake -DFREQLAB_LICENSING_DEV_NOT_ACTIVATED=ON ... # orange
#   cmake -DFREQLAB_LICENSING_DEV_TAMPERED=ON ...      # purple
# Do not force-default any of these - it would leak into unlicensed builds.
if(NOT TARGET freqlab_licensing)
    FetchContent_Declare(
        freqlab_licensing
        GIT_REPOSITORY https://github.com/Nanoshrine-Interactive/freqlab-licensing
        GIT_TAG main
    )
    FetchContent_MakeAvailable(freqlab_licensing)
endif()

# Required JUCE modules
target_link_libraries(Jucerguy
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        freqlab_licensing
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# Compile definitions
target_compile_definitions(Jucerguy
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

# C++17
target_compile_features(Jucerguy PRIVATE cxx_std_17)
