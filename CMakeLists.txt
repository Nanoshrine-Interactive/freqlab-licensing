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

# PIC required when statically linked into a plugin .dylib / .dll / .so.
set_target_properties(freqlab_licensing PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)

# Canonical seller-facing target name. The cloud build replaces the
# underlying target with the real implementation while keeping this alias.
add_library(freqlab::licensing ALIAS freqlab_licensing)
