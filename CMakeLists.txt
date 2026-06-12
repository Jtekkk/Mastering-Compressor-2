cmake_minimum_required(VERSION 3.22)

project(MC2MasteringCompressor VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# ------------------------------------------------------------------ JUCE ----
include(FetchContent)
FetchContent_Declare(
    JUCE
    URL https://github.com/juce-framework/JUCE/archive/refs/tags/8.0.4.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    PATCH_COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/patch-juce.cmake
)
# When cross-compiling, the juce_vst3_helper executable targets Windows and
# cannot run on the build host - skip the optional moduleinfo.json step.
# (Normal variable on purpose: JUCE force-overwrites the cache entry.)
if(CMAKE_CROSSCOMPILING AND CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(JUCE_WINDOWS_HELPERS_CAN_RUN FALSE)
endif()

FetchContent_MakeAvailable(JUCE)

# ---------------------------------------------------------------- Plugin ----
juce_add_plugin(MC2
    PLUGIN_MANUFACTURER_CODE Jtkk
    PLUGIN_CODE Mc2c
    COMPANY_NAME "JTEKK Audio"
    PRODUCT_NAME "MC-2 Mastering Compressor"
    PLUGIN_NAME "MC-2 Mastering Compressor"
    DESCRIPTION "Twin-tube vari-mu mastering compressor"
    FORMATS VST3 AU Standalone
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES "Fx" "Dynamics" "Mastering"
    AU_MAIN_TYPE "kAudioUnitType_Effect"
)

target_sources(MC2 PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
)

target_include_directories(MC2 PRIVATE Source)

target_compile_definitions(MC2 PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    # No media-file codecs needed; also unbuildable with mingw-w64 headers.
    JUCE_USE_WINDOWS_MEDIA_FORMAT=0
)

# LTO stays off for cross builds: MinGW links are slow and fragile with it.
set(MC2_FLAG_TARGETS
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)
if(NOT CMAKE_CROSSCOMPILING)
    list(APPEND MC2_FLAG_TARGETS juce::juce_recommended_lto_flags)
endif()

target_link_libraries(MC2
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        ${MC2_FLAG_TARGETS}
)

# MSVC picks these up from JUCE's #pragma comment(lib, ...) directives;
# MinGW ignores those, so list the Windows system libraries explicitly.
if(WIN32 AND NOT MSVC)
    target_link_libraries(MC2 PRIVATE
        winmm ws2_32 version shlwapi wininet dwmapi uxtheme shcore
        imm32 comdlg32 shell32 ole32 oleaut32 uuid rpcrt4 advapi32
        d2d1 dwrite dxgi dcomp d3d11 windowscodecs oleacc dsound
        dbghelp comctl32 sapi ksuser avrt)

    # MSVC also embeds a ComCtl32-v6 SxS manifest via JUCE's
    # /manifestdependency pragma; without it the loader binds legacy
    # comctl32 5.82 and TaskDialogIndirect resolution fails at load time
    # ("Entry Point Not Found"). Embed equivalent manifests with windres.
    if(TARGET MC2_VST3)
        target_sources(MC2_VST3 PRIVATE cmake/manifest-dll.rc)
    endif()
    if(TARGET MC2_Standalone)
        target_sources(MC2_Standalone PRIVATE cmake/manifest-exe.rc)
    endif()
endif()

# ---------------------------------------------------- Headless DSP tests ----
# The DSP core (Source/DSP) is dependency-free C++17, so it can be exercised
# without a host or audio hardware.
add_executable(dsp_smoke tests/dsp_smoke.cpp)
target_include_directories(dsp_smoke PRIVATE Source)
target_compile_features(dsp_smoke PRIVATE cxx_std_17)

enable_testing()
add_test(NAME dsp_smoke COMMAND dsp_smoke)
