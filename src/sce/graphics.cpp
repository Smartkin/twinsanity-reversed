#include "platform/graphics.h"

// Sony's libgraph call of the retail renderer (FUN_0019b570)
extern "C" void sceGsResetPath()
{
    Platform::Graphics::ResetPath();
}
