/* Case alias for Linux cross-builds: JUCE includes <Dbghelp.h>, mingw-w64
   ships <dbghelp.h>. Harmless on real Windows (case-insensitive FS). */
#include <dbghelp.h>
