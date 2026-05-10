# freqlab_licensing: local-build static library.
#
# Drop this directory under your plugin repo's vendor/ and add:
#
#   add_subdirectory(vendor/freqlab-licensing)
#   target_link_libraries(MyPlugin PRIVATE freqlab::licensing)
#
# The cloud build pipeline rewrites the add_subdirectory argument to point
# at the real SDK at build time. The freqlab::licensing alias is identical
# in both builds, so seller code does not change.

cmake_minimum_required(VERSION 3.16)
project(freqlab_licensing
    VERSION 0.1.0
    DESCRIPTION "Local development surface for freqlab plugin licensing"
    LANGUAGES CXX
)

add_library(freqlab_licensing STATIC
    src/stub.cpp
)

target_include_directories(freqlab_licensing PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_compile_features(freqlab_licensing PUBLIC cxx_std_17)

# Local UI dev-mode toggles. Set exactly one ON to bake a fake status
# into the stub so the seller's licensing UI can render that state
# locally without a cloud round-trip. Default (all OFF) keeps the UI
# hidden, matching unlicensed cloud builds.
#
# If multiple are ON, precedence: LICENSED > EXPIRED > TAMPERED > GRACE
# > TRIAL > NOT_ACTIVATED.
option(FREQLAB_LICENSING_DEV_MODE          "Local dev: NotActivated (alias)" OFF)
option(FREQLAB_LICENSING_DEV_NOT_ACTIVATED "Local dev: NotActivated"          OFF)
option(FREQLAB_LICENSING_DEV_LICENSED      "Local dev: Licensed"              OFF)
option(FREQLAB_LICENSING_DEV_EXPIRED       "Local dev: Expired"               OFF)
option(FREQLAB_LICENSING_DEV_TAMPERED      "Local dev: Tampered"              OFF)
option(FREQLAB_LICENSING_DEV_GRACE         "Local dev: GracePeriod"           OFF)
option(FREQLAB_LICENSING_DEV_TRIAL         "Local dev: Trial"                 OFF)

foreach(_flag IN ITEMS
    FREQLAB_LICENSING_DEV_MODE
    FREQLAB_LICENSING_DEV_NOT_ACTIVATED
    FREQLAB_LICENSING_DEV_LICENSED
    FREQLAB_LICENSING_DEV_EXPIRED
    FREQLAB_LICENSING_DEV_TAMPERED
    FREQLAB_LICENSING_DEV_GRACE
    FREQLAB_LICENSING_DEV_TRIAL
)
    if(${_flag})
        target_compile_definitions(freqlab_licensing PRIVATE ${_flag}=1)
    endif()
endforeach()

# PIC required when statically linked into a plugin .dylib / .dll / .so.
set_target_properties(freqlab_licensing PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)

# Canonical seller-facing target name. The cloud build replaces the
# underlying target with the real implementation while keeping this alias.
add_library(freqlab::licensing ALIAS freqlab_licensing)
