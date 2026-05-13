cmake_minimum_required(VERSION 3.22)

# Toolchain settings must precede project() so the freqlab licensing SDK
# (injected via -DCMAKE_PROJECT_INCLUDE) inherits the same CRT / archs /
# deployment target. Otherwise: Windows LNK2038, macOS single-arch
# binaries, or libc++ "<filesystem> introduced in 10.15" errors.
if(WIN32)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" CACHE INTERNAL "")
endif()

if(APPLE)
    # 10.15 is the SDK's floor (std::filesystem in libc++).
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum macOS version")
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Universal binary")
endif()

project(Iplugtest VERSION 1.0.0)

# Emit compile_commands.json so clangd (LSP) can resolve include paths
# without a custom configuration. Ignored by build; only used by editors.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Allow FetchContent_Populate for selective dependency handling
if(POLICY CMP0169)
    cmake_policy(SET CMP0169 OLD)
endif()

# Fetch dependencies
include(FetchContent)

# CLAP SDK
FetchContent_Declare(
    clap
    GIT_REPOSITORY https://github.com/free-audio/clap.git
    GIT_TAG 1.2.7
)

# CLAP helpers
FetchContent_Declare(
    clap-helpers
    GIT_REPOSITORY https://github.com/free-audio/clap-helpers.git
    GIT_TAG main
)

# VST3 SDK
FetchContent_Declare(
    vst3sdk
    GIT_REPOSITORY https://github.com/steinbergmedia/vst3sdk.git
    GIT_TAG v3.7.11_build_10
)

# iPlug2
FetchContent_Declare(
    iPlug2
    GIT_REPOSITORY https://github.com/iPlug2/iPlug2.git
    GIT_TAG master
)

# Populate CLAP SDK and helpers
FetchContent_MakeAvailable(clap clap-helpers)

# Populate VST3 SDK (don't add as subdirectory)
FetchContent_GetProperties(vst3sdk)
if(NOT vst3sdk_POPULATED)
    FetchContent_Populate(vst3sdk)
endif()

# Populate iPlug2 (don't add subdirectory - skip examples/tests)
FetchContent_GetProperties(iPlug2)
if(NOT iplug2_POPULATED)
    FetchContent_Populate(iPlug2)
endif()

# Set up SDK paths for iPlug2
set(IPLUG_DEPS_DIR ${iplug2_SOURCE_DIR}/Dependencies/IPlug CACHE PATH "" FORCE)
file(MAKE_DIRECTORY ${IPLUG_DEPS_DIR}/CLAP_SDK)
file(MAKE_DIRECTORY ${IPLUG_DEPS_DIR}/CLAP_HELPERS)
file(MAKE_DIRECTORY ${IPLUG_DEPS_DIR}/VST3_SDK)

# Create symlinks to SDK locations
if(NOT EXISTS ${IPLUG_DEPS_DIR}/CLAP_SDK/include)
    file(CREATE_LINK ${clap_SOURCE_DIR}/include ${IPLUG_DEPS_DIR}/CLAP_SDK/include SYMBOLIC)
endif()
if(NOT EXISTS ${IPLUG_DEPS_DIR}/CLAP_HELPERS/include)
    file(CREATE_LINK ${clap-helpers_SOURCE_DIR}/include ${IPLUG_DEPS_DIR}/CLAP_HELPERS/include SYMBOLIC)
endif()
if(NOT EXISTS ${IPLUG_DEPS_DIR}/VST3_SDK/base)
    file(CREATE_LINK ${vst3sdk_SOURCE_DIR}/base ${IPLUG_DEPS_DIR}/VST3_SDK/base SYMBOLIC)
endif()
if(NOT EXISTS ${IPLUG_DEPS_DIR}/VST3_SDK/pluginterfaces)
    file(CREATE_LINK ${vst3sdk_SOURCE_DIR}/pluginterfaces ${IPLUG_DEPS_DIR}/VST3_SDK/pluginterfaces SYMBOLIC)
endif()
if(NOT EXISTS ${IPLUG_DEPS_DIR}/VST3_SDK/public.sdk)
    file(CREATE_LINK ${vst3sdk_SOURCE_DIR}/public.sdk ${IPLUG_DEPS_DIR}/VST3_SDK/public.sdk SYMBOLIC)
endif()

# AAX SDK (optional - pass -DAAX_SDK_PATH=<path> to enable AAX builds)
if(DEFINED AAX_SDK_PATH AND EXISTS "${AAX_SDK_PATH}")
    file(MAKE_DIRECTORY ${IPLUG_DEPS_DIR}/AAX_SDK)
    if(NOT EXISTS ${IPLUG_DEPS_DIR}/AAX_SDK/Interfaces AND EXISTS ${AAX_SDK_PATH}/Interfaces)
        file(CREATE_LINK ${AAX_SDK_PATH}/Interfaces ${IPLUG_DEPS_DIR}/AAX_SDK/Interfaces SYMBOLIC)
    endif()
    if(NOT EXISTS ${IPLUG_DEPS_DIR}/AAX_SDK/Libs AND EXISTS ${AAX_SDK_PATH}/Libs)
        file(CREATE_LINK ${AAX_SDK_PATH}/Libs ${IPLUG_DEPS_DIR}/AAX_SDK/Libs SYMBOLIC)
    endif()
endif()

# Set up iPlug2 CMake module
set(IPLUG2_DIR ${iplug2_SOURCE_DIR} CACHE PATH "iPlug2 root directory" FORCE)
include(${IPLUG2_DIR}/iPlug2.cmake)

# Don't auto-install to system plugin dirs - freqlab handles publishing
set(IPLUG_DEPLOY_PLUGINS OFF CACHE BOOL "" FORCE)

find_package(iPlug2 REQUIRED)

# freqlab licensing.
#
# Cloud licensed builds inject the real SDK via CMAKE_PROJECT_INCLUDE
# during project(); the guard avoids a duplicate-target error. Local +
# cloud-unlicensed builds FetchContent the stub, which reports
# Status::NoConfig - the documented signal to hide licensing UI.
# Dev flags: -DFREQLAB_LICENSING_DEV_LICENSED=ON (and EXPIRED /
# NOT_ACTIVATED / TAMPERED) iterate UI states locally. Do NOT
# force-default these here.
if(NOT TARGET freqlab_licensing)
    include(FetchContent)
    FetchContent_Declare(
        freqlab_licensing
        GIT_REPOSITORY https://github.com/Nanoshrine-Interactive/freqlab-licensing
        GIT_TAG main
    )
    FetchContent_MakeAvailable(freqlab_licensing)
endif()

# Create plugin targets with IGraphics UI
iplug_add_plugin(${PROJECT_NAME}
    SOURCES
        src/PluginProcessor.cpp
        src/PluginProcessor.h
        src/LicenseUI.h
        src/freqlab_response.h
        config.h
        resource.h
    FORMATS VST3 CLAP
    UI IGRAPHICS
)

# Suppress warnings from iPlug2 upstream code (VLA extensions in RtAudio/RtMidi/SWELL/AUv2,
# incomplete switch in SWELL) - these are in iPlug2's own sources, not user code
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|AppleClang")
    foreach(_target ${PROJECT_NAME}-app ${PROJECT_NAME}-vst3 ${PROJECT_NAME}-clap ${PROJECT_NAME}-au)
        if(TARGET ${_target})
            target_compile_options(${_target} PRIVATE -Wno-vla-cxx-extension -Wno-switch)
        endif()
    endforeach()
endif()

# Link freqlab licensing into every plugin-shaped target. iPlug2 creates
# one target per format ({name}-vst3, -clap, -app, -au); link to whichever
# exist in this configuration.
foreach(_target ${PROJECT_NAME}-app ${PROJECT_NAME}-vst3 ${PROJECT_NAME}-clap ${PROJECT_NAME}-au)
    if(TARGET ${_target})
        target_link_libraries(${_target} PRIVATE freqlab_licensing)
    endif()
endforeach()
