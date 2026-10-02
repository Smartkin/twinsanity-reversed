#include "common.h"
#include "retail/libc.h"

// C library functions PS2SDK's libraries call that the game's own C library (Sony's newlib, in the asm) has no symbol for. The
// rest are ps2sdk.txt's aliases
extern "C"
{
    int scePrintf(const char* format, ...) RETAIL(DebugPrint_);

    int puts(const char* text)
    {
        return scePrintf("%s\n", text);
    }

    // BSD's, which newer PS2SDKs copy their paths and names with: what fits of the text, always ended, and the text's length
    size_t strlcpy(char* destination, const char* source, size_t size)
    {
        size_t length = RetailLibc::StringLength(source);
        if (size != 0)
        {
            size_t count = length < size ? length : size - 1;
            RetailLibc::MemoryCopy(destination, source, count);
            destination[count] = '\0';
        }

        return length;
    }
}
