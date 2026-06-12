# Cross-compile from Linux to 64-bit Windows using llvm-mingw
# (https://github.com/mstorsjo/llvm-mingw — clang + lld + current mingw-w64,
# UCRT). Plain GCC/MinGW cannot build JUCE 8: its Direct2D code uses
# _Pragma inside default member initializers, which GCC rejects, and
# distro mingw-w64 headers predate Direct2D 1.3.
#
#   1. Unpack an llvm-mingw ucrt release for your host into /opt/llvm-mingw
#      (or set LLVM_MINGW_ROOT).
#   2. cmake -B build-win -DCMAKE_BUILD_TYPE=Release \
#            -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-llvm-mingw-w64.cmake
#   3. cmake --build build-win --target MC2_VST3
#
# The result is self-contained (static runtimes): no extra DLLs to ship.

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

if(NOT DEFINED LLVM_MINGW_ROOT)
    if(DEFINED ENV{LLVM_MINGW_ROOT})
        set(LLVM_MINGW_ROOT "$ENV{LLVM_MINGW_ROOT}")
    else()
        set(LLVM_MINGW_ROOT "/opt/llvm-mingw")
    endif()
endif()

set(CMAKE_C_COMPILER   "${LLVM_MINGW_ROOT}/bin/x86_64-w64-mingw32-clang")
set(CMAKE_CXX_COMPILER "${LLVM_MINGW_ROOT}/bin/x86_64-w64-mingw32-clang++")
set(CMAKE_RC_COMPILER  "${LLVM_MINGW_ROOT}/bin/x86_64-w64-mingw32-windres")

set(CMAKE_FIND_ROOT_PATH "${LLVM_MINGW_ROOT}/x86_64-w64-mingw32")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Small SDK-compatibility shim (see mingw-compat.h) force-included everywhere,
# plus case-aliases for headers JUCE includes with Windows-SDK capitalisation.
set(CMAKE_C_FLAGS_INIT   "-include ${CMAKE_CURRENT_LIST_DIR}/mingw-compat.h -isystem ${CMAKE_CURRENT_LIST_DIR}/win-include-aliases")
set(CMAKE_CXX_FLAGS_INIT "-include ${CMAKE_CURRENT_LIST_DIR}/mingw-compat.h -isystem ${CMAKE_CURRENT_LIST_DIR}/win-include-aliases")

# Fold the runtimes (libc++, libunwind, winpthread) into the binaries so the
# .vst3 and .exe have no toolchain DLL dependencies.
set(CMAKE_EXE_LINKER_FLAGS_INIT    "-static")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "-static")
set(CMAKE_MODULE_LINKER_FLAGS_INIT "-static")

# Note: the cross-compiled juce_vst3_helper.exe cannot run on the Linux host;
# CMakeLists.txt disables JUCE's optional moduleinfo.json step when
# cross-compiling (JUCE force-overwrites the cache entry, so the project sets
# it as a normal variable instead).
