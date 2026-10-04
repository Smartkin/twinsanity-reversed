#include "platform/sound.h"

// SDRDRV's EE side (sdr.cpp)
extern "C" int sceSdRemoteInit();

s32 Platform::Sound::InitialiseRemote()
{
    return sceSdRemoteInit();
}
