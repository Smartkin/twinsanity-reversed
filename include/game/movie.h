#pragma once

#include "common.h"
#include "gcc2.h"
#include "game/string.h"
#include "platform/movie.h"

// The movies (FMVs). The game asks for one to play or stop, and the controller's Update, once a frame, takes the request on and
// starts, steps or stops it. The controller is abstract (its vtable's functions but the last two and the destructor are
// pure): MoviePlayer plays the movies, on the platform's player. Its vtable follows 0x18 bytes of members
class GameMovieController
{
public:
    enum Flags : u32
    {
        AudioChannelMask = 0x3F,
        // The movie's pictures are 16:9
        Widescreen = 0x40,
        // The movie has the sound (LendSound)
        SoundLent = 0x80,
        // What the game asked for, which Update takes on as the state
        RequestMask = 0xF00,
        PlayRequested = 0x100,
        StopRequested = 0x300,
        NothingRequested = 0x400,
        StateMask = 0xF000,
        StateStarting = 0x1000,
        StatePlaying = 0x2000,
        StateStopping = 0x3000,
    };

    u32 flags;
    s32 width;
    s32 height;
    // The movie's file, without its extension
    String path;
    const GccVTableEntry* vtable;

    static GameMovieController* Construct(GameMovieController* controller) RETAIL(FUN_002b02d0);
    void Destroy(u32 flags) RETAIL(FUN_002b0340);

    bool IsPlaying() const
    {
        return (flags & StateMask) == StatePlaying;
    }

    // Asks for the movie to play, nothing while one does: its file, the channel of its sound, whether it's 16:9 (bit 0) and its
    // size. Returns whether it asked
    u32 Play(const char* file, u32 audioChannel, u32 widescreen, s32 width, s32 height) RETAIL(FUN_002b0398);
    // Asks for the movie playing to stop. Returns whether one plays
    u32 RequestStop() RETAIL(FUN_002b0448);
    // Starts, steps or stops the movie. Returns 1 while one plays
    u32 Update() RETAIL(FUN_002af958);
    // The base class's sound hand-over: the flag alone
    void BaseLendSound() RETAIL(FUN_002afd48);
    void BaseReclaimSound() RETAIL(FUN_002afd58);

    // The virtual functions. Start and Step return whether the movie plays (on)
    u32 Start()
    {
        return CallVirtual<u32>(this, vtable, 1);
    }

    u32 Stop()
    {
        return CallVirtual<u32>(this, vtable, 2);
    }

    u32 Step()
    {
        return CallVirtual<u32>(this, vtable, 4);
    }

    // The picture into the frame's render buckets (from the renderer's frame)
    void Draw()
    {
        CallVirtual<void>(this, vtable, 5);
    }

    // The frame's render while a movie plays: waits until a picture is presented
    void WaitFrame()
    {
        CallVirtual<void>(this, vtable, 6);
    }

    // Sends the next decoded picture to the GS (from the vertical blank's interrupt)
    void QueuePicture()
    {
        CallVirtual<void>(this, vtable, 7);
    }

    // While a movie plays its sound is the movie's
    s32 LendSound()
    {
        return CallVirtual<s32>(this, vtable, 8);
    }

    void ReclaimSound()
    {
        CallVirtual<void>(this, vtable, 9);
    }
};
CHECK_SIZE(GameMovieController, 0x1C);

// The movie player: the controller, the files' extension and the platform's player
class MoviePlayer : public GameMovieController
{
public:
    // The retail player's GS address of the pictures, which the platform's player keeps
    u32 unknown1C;
    String extension;
    alignas(16) u8 player[Platform::Movie::PlayerSize];

    static MoviePlayer* Construct(MoviePlayer* player) RETAIL(FUN_002b0050);

    // Its vtable's functions
    u32 Start() RETAIL(FUN_002aeb98);
    // Gives the screen back to the renderer and the sound back to the game
    u32 Stop() RETAIL(Render_002AEED8);
    void Destroy(u32 flags) RETAIL(FUN_002afd80);
    u32 Step() RETAIL(FUN_002af288);
    void Draw() RETAIL(FUN_002af3b0);
    void WaitFrame() RETAIL(FUN_002b0170);
    void QueuePicture() RETAIL(FUN_002b01e8);
    // The reverbs off and the sound processor's transfers lent to the movie (game/sound.cpp)
    s32 LendSound() RETAIL(FUN_002b0238);
    void ReclaimSound() RETAIL(FUN_002b0280);

    Platform::Movie::Player* PlatformPlayer()
    {
        return reinterpret_cast<Platform::Movie::Player*>(player);
    }
};
CHECK_OFFSET(MoviePlayer, extension, 0x20);
CHECK_OFFSET(MoviePlayer, player, 0x30);
CHECK_SIZE(MoviePlayer, 0x4200);
