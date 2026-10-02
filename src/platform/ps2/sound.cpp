#include "platform/sound.h"

// Sony's libsdr, which the retail asm still has: the installed PS2SDK has no EE side for SDRDRV (PS2SDK's sources have one,
// ee/rpc/sdr)
extern "C" int sceSdRemoteInit();

s32 Platform::Sound::InitialiseRemote()
{
    return sceSdRemoteInit();
}
