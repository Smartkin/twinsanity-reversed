#pragma once

#include "common.h"

// The platform layer: what the game needs of the machine it runs on. The game's C++ calls these interfaces and a platform
// implements them (src/platform/<platform>/, picked by configure.py). The PS2 one is on PS2SDK; the retail asm that's left calls
// Sony's SDK by name, which src/sce/ puts on the same interfaces. The platform's entry sets things up and calls Main.
namespace Platform::System
{
// Makes the machine ready for the game, first thing: on the PS2 the graphics hardware reset and the I/O processor restarted with
// the disc's modules
void Initialise();
// Starts what the pads, the memory cards and the sound go through
void StartServices();
[[noreturn]] void Exit(s32 status);

// The console's languages (the PS2's system settings')
enum ConsoleLanguage : s32
{
    LanguageJapanese = 0,
    LanguageEnglish = 1,
    LanguageFrench = 2,
    LanguageSpanish = 3,
    LanguageGerman = 4,
    LanguageItalian = 5,
    LanguageDutch = 6,
    LanguagePortuguese = 7,
};

ConsoleLanguage Language();

struct DateTime
{
    u16 year;
    // From 1
    u8 month;
    u8 day;
    u8 hour;
    u8 minute;
    u8 second;
};

// The local date and time
DateTime LocalTime();
}
