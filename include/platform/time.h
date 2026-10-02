#pragma once

#include "common.h"

// The clock the game's frames go by
namespace Platform::Time
{
// The PS2's timer 0, at the bus clock / 256
constexpr u32 TicksPerSecond = 576000;

// Starts it counting from 0, the first time
void Initialise();
// The ticks since then
u64 Ticks();
}
