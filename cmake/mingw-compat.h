/* Force-included into every TU by toolchain-llvm-mingw-w64.cmake.

   Bridges small gaps between the Windows SDK that JUCE 8 expects and the
   mingw-w64 headers shipped with llvm-mingw. */

#pragma once

/* JUCE pulls <cstring> in transitively on MSVC but not on MinGW. */
#ifdef __cplusplus
 #include <cstring>
#else
 #include <string.h>
#endif

/* mingw-w64's uiautomationcore.h does not declare the CaretPosition
   enumeration (UI Automation, Windows 8.1 SDK). Values match the SDK. */
typedef enum CaretPosition
{
    CaretPosition_Unknown         = 0,
    CaretPosition_EndOfLine       = 1,
    CaretPosition_BeginningOfLine = 2
} CaretPosition;
