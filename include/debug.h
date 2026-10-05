#pragma once

#include "common.h"

// The test hooks' lists: results to watch, game controller states logged, frames of random numbers kept, inputs given
constexpr u32 DebugValueCount = 16;
constexpr u32 DebugStateCount = 64;
constexpr u32 DebugRandomFrames = 4096;
constexpr u32 DebugInputCount = 16;

// A word the code writes as it gets along, for tools/run_pcsx2.py --watch g_DebugStep to follow over PINE
extern "C" volatile u32 g_DebugStep;
// Results worth a look, for --watch g_DebugValues+N
extern "C" volatile s32 g_DebugValues[DebugValueCount];

#define DEBUG_STEP(step) (g_DebugStep = (step))

// What tools/run_pcsx2.py does at an exact frame (--at-frame), so runs of two builds can be compared frame by frame: the frame
// a game controller state is asked for or pad 1's buttons are held, and the frame the game stops after (its screen is that
// frame's, for a snapshot) until the runner changes it
struct DebugInput
{
    enum Kind : u32
    {
        None = 0,
        // The game controller's next state: value
        State = 1,
        // Pad 1's buttons (the PS2 pad layer's g_TestPadButtons) held for length frames: value
        Press = 2,
    };

    u32 frame;
    Kind kind;
    u32 value;
    u32 length;
};

extern "C" volatile u32 g_DebugFrame;
extern "C" volatile u32 g_DebugFreezeFrame;
// The game controller's states as they changed: the frame << 8 | the state (tools/run_pcsx2.py prints them at the end)
extern "C" volatile u32 g_DebugStates[DebugStateCount];
// Every frame's state of the C library's random numbers (rand's), by frame, for comparing runs
extern "C" volatile u32 g_DebugRandom[DebugRandomFrames];
extern "C" volatile DebugInput g_DebugInputs[DebugInputCount];
// Pad 1's buttons held for tests (the report's button word, pressed 1), which a platform's pad layer defines and applies
extern "C" volatile u32 g_TestPadButtons;

// After a frame was presented: counts it, applies its inputs, stops while it's the frame to stop after
void DebugFrameRendered();

// Test runs' time (tools/render_check.py sets it in the copy of the ELF it runs, it's in .data for that): every frame takes
// exactly the frame's time, and a frame's work goes on for a set number of time checks (clock.cpp), so what a frame draws doesn't
// depend on how fast the code is
extern "C" volatile u32 g_DebugFixedTime;
