#include "common.h"

// C library functions PS2SDK's libraries call that the game's own C library (Sony's newlib, in the asm) has no symbol for. The
// rest are ps2sdk.txt's aliases
extern "C"
{
    int scePrintf(const char* format, ...) RETAIL(DebugPrint_);

    int puts(const char* text)
    {
        return scePrintf("%s\n", text);
    }
}
