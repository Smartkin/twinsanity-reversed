#pragma once

#include "common.h"

// The movies (FMVs): a player decodes a movie file's pictures and plays its sound while the game shows them. The PS2's plays PSS
// files from the disc: MPEG-2 pictures decoded on the IPU, sent to the GS from the vertical blank's interrupt, and PCM sound
// streamed to the sound processor
namespace Platform::Movie
{
// A player's state, kept in the game's movie controller (16 byte aligned, the PS2's making the controller 0x4200 bytes)
struct Player;
constexpr u32 PlayerSize = 0x41D0;

// The memory the game can lend a player to stream through, which it doesn't use while the movie plays (on the PS2 a music
// stream's buffer in the I/O processor's memory)
constexpr u32 LentMemorySize = 0x2E040;

// The disc's name of a movie file is FilePrefix, the game's path, "." and Extension, then FileSuffix, in capitals when
// CapitalFileNames (the PS2's ISO 9660 names: "\\FMV\\INTRO.PSS;1")
inline constexpr char FilePrefix[] = "\\";
inline constexpr char Extension[] = "pss";
inline constexpr char FileSuffix[] = ";1";
constexpr bool CapitalFileNames = true;

void Construct(Player* player);

// The screen is the player's from the next vertical blank on: while WaitFrame waits, every second one calls present (the game's
// present of the frame it drew, which then has the player queue the next picture)
void BeginPresenting(Player* player, void (*present)());
// Opens the movie file (its disc name), decodes its first pictures and fills the sound's buffer with the audio channel's sound.
// lentMemory is LentMemorySize bytes the game lends the player until Close, or nullptr. Returns false when it couldn't open it
bool Open(Player* player, const char* file, u32 audioChannel, s32 width, void* lentMemory);
// Starts the sound, at the volume (0 to 1)
void StartSound(Player* player, f32 volume);
// Decodes the next picture when one waits for less than two others to be shown, and feeds the sound. Returns false once the movie
// has ended
bool Step(Player* player);
// Waits until a picture has been presented, reading on meanwhile
void WaitFrame(Player* player);
// Sends the next decoded picture to the screen's memory (from present)
void QueuePicture(Player* player);

// An area of the screen, in pixels
struct Area
{
    s32 x;
    s32 y;
    s32 width;
    s32 height;
};

// Draws the picture last sent over the area: the movie's width by height pixels, without skippedRows rows at the top and the
// bottom. Nothing until the player has decoded two pictures since Open. The PS2's draws into the renderer's movie bucket (27)
void Draw(Player* player, const Area& area, s32 width, s32 height, u32 skippedRows);

// Stops presenting and closes the movie. The memory the game lent is the game's again
void Close(Player* player);
}
