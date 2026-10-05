#include "common.h"
#include "gcc2.h"
#include "game/context.h"
#include "game/controllers.h"
#include "game/filestream.h"
#include "game/stream.h"
#include "game/memory.h"
#include "game/movie.h"
#include "game/string.h"
#include "platform/system.h"
#include "retail/libc.h"

namespace
{
constexpr u32 MaxArguments = 256;
// The file streams' channels
constexpr s32 StreamChannels = 9;
// The game's clock goes by the display's frames: 50 a second on a PAL TV, 60 on an NTSC one
constexpr s32 PalFramesPerSecond = 50;
constexpr s32 NtscFramesPerSecond = 60;
// One pad, so pad 1's bit (of GamePadController's bit per pad made) is never set
constexpr s32 PadCount = 1;
constexpr u32 SecondPadBit = 1u << 1;

bool IsPlayingMovie(const GameMovieController* movie)
{
    return movie != nullptr && movie->IsPlaying();
}
}

extern "C"
{
    // "rb batch=Crash6\Crash": the arguments the game takes when it's given none. ParseArguments cuts the words out of it
    extern char* g_DefaultArguments RETAIL(D_00309884);
}

extern "C" int Main(u32 argc, char** argv)
{
    RunStaticConstructors();
    Platform::System::Initialise();
    GameContext* context = ParseArguments(argc, argv);
    Platform::System::StartServices();
    InitStreamSystem(StreamChannels);
    CreateMusic();
    UpdateStreamSystem(g_StreamSystem);

    if (G_GameRendererController == nullptr)
    {
        void* memory = MemoryAllocate(sizeof(GameRendererController));
        G_GameRendererController = GameRendererController::Construct(memory, g_ScreenWidth, g_ScreenHeight, g_Pal);
    }

    if (G_GameClockController == nullptr)
    {
        G_GameClockController = GameTimeConstruct(static_cast<GameTimeController*>(MemoryAllocate(sizeof(GameTimeController))),
                                                  g_Pal ? PalFramesPerSecond : NtscFramesPerSecond);
    }

    if (G_GamePadController == nullptr)
    {
        GamePadController* pads = GamePadController::Construct(MemoryAllocate(sizeof(GamePadController)), PadCount);
        pads->flags.pads &= ~SecondPadBit;
        G_GamePadController = pads;
    }

    context->StartUp();
    while (g_GameState != GameStateQuitting)
    {
        // Each step asks again, the movie can start or end in between
        GameMovieController* movie = G_GameMovieController;
        context->BeginFrame(IsPlayingMovie(movie));
        context->Update(IsPlayingMovie(movie));
        context->EndFrame(IsPlayingMovie(movie));
        context->Render(IsPlayingMovie(movie));
    }

    return 0;
}

// The launch arguments, made into the game's context once
extern "C" GameContext* ParseArguments(u32 argc, char** argv)
{
    if (g_GameContext != nullptr)
    {
        return g_GameContext;
    }

    char* words[MaxArguments];
    u32 count = 0;
    if (argc < 2)
    {
        // The game's own arguments, split at white space in place. Like a program's name, words[0] is left out. The file
        // made around it is never opened: what's left of reading them from one
        File file;
        File::Construct(&file);
        char* text = g_DefaultArguments;
        u32 length = RetailLibc::StringLength(text);
        u32 wordStart = 0;
        bool betweenWords = true;
        count = 1;
        for (u32 i = 0; i < length; i++)
        {
            char character = text[i];
            if (character == '\n' || character == '\r' || character == '\t' || character == ' ')
            {
                if (!betweenWords)
                {
                    text[i] = '\0';
                    words[count++] = text + wordStart;
                }

                wordStart = i + 1;
                betweenWords = true;
            }
            else
            {
                betweenWords = false;
            }
        }

        if (!betweenWords)
        {
            words[count++] = text + wordStart;
        }

        file.Destroy(DestroyOnly);
    }
    else
    {
        do
        {
            words[count] = argv[count];
            count++;
        } while (count < argc && count < MaxArguments);
    }

    g_GameContext = CreateGameContext(count, words);
    return g_GameContext;
}
