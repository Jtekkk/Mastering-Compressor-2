# Applied to the fetched JUCE sources via FetchContent PATCH_COMMAND.
#
# JUCE 8.0.4 ships a legacy AudioPluginInstance constructor that delegates a
# decayed array parameter to an AudioProcessor constructor that no longer
# exists. MSVC/GCC only notice on instantiation (never instantiated), but
# clang >= 22 rejects it at parse time, breaking the llvm-mingw Windows
# cross-build. The constructor is host-facing API unused by plugin targets,
# so it is simply removed. Idempotent.

set(header "modules/juce_audio_processors/processors/juce_AudioPluginInstance.h")

file(READ "${header}" contents)

string(REPLACE
"    template <size_t numLayouts>
    AudioPluginInstance (const short channelLayoutList[numLayouts][2]) : AudioProcessor (channelLayoutList) {}
"
"" patched "${contents}")

file(WRITE "${header}" "${patched}")

# MSVC's __uuidof accepts ComSmartPtr expressions via its compiler magic;
# mingw-w64's __uuidof emulation needs the interface type spelled out.
# The explicit form is equally valid under MSVC.
set(d2dres "modules/juce_graphics/native/juce_Direct2DResources_windows.cpp")

file(READ "${d2dres}" contents)
string(REPLACE "surface->GetDevice (__uuidof (device),"
               "surface->GetDevice (__uuidof (IDXGIDevice),"   contents "${contents}")
string(REPLACE "chain->GetDevice (__uuidof (device),"
               "chain->GetDevice (__uuidof (IDXGIDevice),"     contents "${contents}")
string(REPLACE "chain->GetBuffer (0, __uuidof (surface),"
               "chain->GetBuffer (0, __uuidof (IDXGISurface)," contents "${contents}")
file(WRITE "${d2dres}" "${contents}")
