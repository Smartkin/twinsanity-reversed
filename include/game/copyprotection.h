#pragma once

#include "common.h"

// The copy protection: a countdown of frames the system module's static initialisation arms and the copyright notice's checksum
// disarms; when it runs out, an instruction of the game's code is broken
extern "C"
{
    // The copyright notice's characters added up: the countdown disarmed when they make the sum they should
    void SimpleCopyrightChecksum() RETAIL(SimpleCopyrightChecksum_);
    // A frame of it (the game's update calls it): the countdown stepped, and whether a word of memory after it (another one every
    // frame) is below 0x58, which nothing reads
    u32 CopyProtectionStep() RETAIL(FUN_00181c70);
}
