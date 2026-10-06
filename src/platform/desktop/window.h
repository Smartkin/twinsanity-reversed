#pragma once

#include "common.h"

// The desktop's window (SDL2): opened when the renderer starts, its events handled at every vertical blank (whether it was
// closed), every frame presented in it, so far as its clear color, and closed when the program exits (before the exit: the GL
// drivers' own exit handlers let go of what SDL's renderer would still call). Built without SDL2 there's none, and these do
// nothing. Its file is built for the host's own layouts (without -malign-double, SDL's structs as the SDL library has them), so
// it uses none of the game's
namespace DesktopWindow
{
void Open();
bool HandleEvents();
void Show(u8 red, u8 green, u8 blue);
void Close();
}
