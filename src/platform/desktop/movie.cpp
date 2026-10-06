#include "platform/movie.h"

// The desktop's side of Platform::Movie: no movies yet
namespace Platform::Movie
{
void Construct(Player* player)
{
}

void BeginPresenting(Player* player, void (*present)())
{
}

bool Open(Player* player, const char* file, u32 audioChannel, s32 width, void* lentMemory)
{
    return false;
}

void StartSound(Player* player, f32 volume)
{
}

bool Step(Player* player)
{
    return false;
}

void WaitFrame(Player* player)
{
}

void QueuePicture(Player* player)
{
}

void Draw(Player* player, const Area& area, s32 width, s32 height, u32 skippedRows)
{
}

void Close(Player* player)
{
}

}
