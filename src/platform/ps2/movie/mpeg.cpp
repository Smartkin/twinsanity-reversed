#include "mpeg.h"

#include "retail/libc.h"

#include <stdio.h>

// libmpeg's set-up, callbacks, work memory and errors

extern "C"
{
    extern const char g_MpegUnknownBufferType[] RETAIL(D_00307380);
    extern const char g_MpegAlignmentError[] RETAIL(D_00307398);
    extern const char g_MpegWorkTooSmall[] RETAIL(D_003073E8);
    extern const char g_MpegArenaTooSmall[] RETAIL(D_00307508);
    extern const char g_MpegBlockDecodeError[] RETAIL(D_003075D0);
    extern const char g_MpegErrorFormat[] RETAIL(D_003075F0);
}

namespace
{
// sceMpegCreate's check of the state's alignment, which Sony's buffers in it need
constexpr u32 StateAlignment = 0x40;
}

namespace Libmpeg
{
void SetBuffers(MpegSystem* sys)
{
    IpuControlRegister control = {*IpuControl};
    control.mpeg1 = 1;
    *IpuControl = control.value;
    if (sys->bufferType == MpegBuffersScratchpad)
    {
        sys->prediction = reinterpret_cast<u8*>(Scratchpad + 2 * ScratchpadBufferBytes);
        sys->buffers[0].references = reinterpret_cast<u8*>(Scratchpad);
        sys->buffers[0].macroblocks = reinterpret_cast<u8*>(Scratchpad + ScratchpadReferencesBytes);
        sys->buffers[1].references = reinterpret_cast<u8*>(Scratchpad + ScratchpadBufferBytes);
        sys->buffers[1].macroblocks = reinterpret_cast<u8*>(Scratchpad + ScratchpadBufferBytes + ScratchpadReferencesBytes);
        sys->bufferIndex = 0;
        return;
    }

    if (sys->bufferType == MpegBuffersOwn)
    {
        sys->prediction = sys->ownPrediction;
        sys->buffers[0].macroblocks = reinterpret_cast<u8*>(
            (reinterpret_cast<u32>(sys->ownMacroblocks[0]) & PhysicalMask) | UncachedAcceleratedSegment);
        sys->buffers[1].macroblocks = reinterpret_cast<u8*>(
            (reinterpret_cast<u32>(sys->ownMacroblocks[1]) & PhysicalMask) | UncachedAcceleratedSegment);
        sys->bufferIndex = 0;
        sys->buffers[0].references = nullptr;
        sys->buffers[1].references = nullptr;
        return;
    }

    Error(sys, g_MpegUnknownBufferType);
}

void StopDecoding(MpegSystem* sys)
{
    sys->bitsStale = 1;
    sys->dcReset = 0;
    s32 interrupts = DIntr();
    *R_EE_D_ENABLEW = *R_EE_D_ENABLER | DmaSuspend;
    *R_EE_D3_CHCR = 0;
    *R_EE_D4_CHCR = 0;
    *R_EE_D9_CHCR = 0;
    *R_EE_D_ENABLEW = *R_EE_D_ENABLER & ~DmaSuspend;
    if (interrupts != 0)
    {
        EIntr();
    }

    *R_EE_D3_QWC = 0;
    *R_EE_D4_QWC = 0;
    *R_EE_D9_QWC = 0;
    *IpuControl = IpuControlReset;
    sceIpuSync(IpuSyncWait, 0);
}

s32 DispatchCallback(Mpeg* mpeg, MpegCallbackData* callbackData)
{
    if (mpeg == nullptr)
    {
        return 0;
    }

    MpegSystem* sys = mpeg->sys;
    if (sys == nullptr)
    {
        return 0;
    }

    MpegCallbackEntry* entry = &sys->callbacks[callbackData->type];
    if (entry->function == nullptr)
    {
        return 0;
    }

    return CallWithGlobalPointer(entry->globalPointer, entry->function, mpeg, callbackData, entry->user);
}

s32 DispatchNoData(Mpeg* mpeg)
{
    MpegCallbackData noData = {MpegCallbackNoData};
    if (mpeg == nullptr)
    {
        return 1;
    }

    MpegSystem* sys = mpeg->sys;
    if (sys == nullptr)
    {
        return 1;
    }

    MpegCallbackEntry* entry = &sys->callbacks[MpegCallbackNoData];
    if (entry->function != nullptr)
    {
        CallWithGlobalPointer(entry->globalPointer, entry->function, mpeg, &noData, entry->user);
    }

    return 1;
}

void ArenaInitialise(MpegArena* arena, u8* base, s32 size)
{
    arena->mark = base;
    arena->size = size;
    arena->base = base;
    arena->next = base;
}

void ArenaMark(MpegArena* arena)
{
    arena->mark = arena->next;
}

void ArenaRewind(MpegArena* arena)
{
    arena->next = arena->mark;
}

u8* Allocate(MpegSystem* sys, MpegArena* arena, s32 size, s32 alignment)
{
    u32 memory = (reinterpret_cast<u32>(arena->next) + alignment - 1) / static_cast<u32>(alignment) * alignment;
    u32 end = memory + size;
    if (reinterpret_cast<u32>(arena->base) + arena->size < end)
    {
        Error(sys, g_MpegArenaTooSmall);
        return nullptr;
    }

    arena->next = reinterpret_cast<u8*>(end);
    return reinterpret_cast<u8*>(memory);
}

s32 StopDma(Mpeg* mpeg, void*, void*)
{
    sceIpuStopDMA(&mpeg->sys->ipuDma);
    return 1;
}

s32 RestartDma(Mpeg* mpeg, void*, void*)
{
    sceIpuRestartDMA(&mpeg->sys->ipuDma);
    return 1;
}

s32 PrintError(const char* message)
{
    return printf(g_MpegErrorFormat, message);
}

void StopIpuDma(MpegSystem*)
{
    s32 interrupts = DIntr();
    *R_EE_D_ENABLEW = *R_EE_D_ENABLER | DmaSuspend;
    *R_EE_D3_CHCR = 0;
    *R_EE_D4_CHCR = 0;
    *R_EE_D_ENABLEW = *R_EE_D_ENABLER & ~DmaSuspend;
    if (interrupts != 0)
    {
        EIntr();
    }

    *R_EE_D3_QWC = 0;
    *R_EE_D4_QWC = 0;
    *IpuControl = IpuControlReset;
}

void BlockDecodeError(MpegSystem* sys)
{
    Error(sys, g_MpegBlockDecodeError);
    MpegCallbackData dma = {MpegCallbackStopDma};
    DispatchCallback(sys->mpeg, &dma);
    *IpuControl = IpuControlReset;
    dma.type = MpegCallbackRestartDma;
    DispatchCallback(sys->mpeg, &dma);
    s32 interrupts = DIntr();
    *R_EE_D_ENABLEW = *R_EE_D_ENABLER | DmaSuspend;
    *R_EE_D3_CHCR = 0;
    *R_EE_D_ENABLEW = *R_EE_D_ENABLER & ~DmaSuspend;
    if (interrupts != 0)
    {
        EIntr();
    }

    *R_EE_D3_QWC = 0;
}

s32 Error(MpegSystem* sys, const char* message)
{
    // The state's Mpeg is read before the state is checked for null
    Mpeg* mpeg = sys->mpeg;
    if (mpeg != nullptr && sys != nullptr && sys->callbacks[MpegCallbackError].function != nullptr)
    {
        MpegErrorData error = {MpegCallbackError, message};
        return DispatchCallback(mpeg, reinterpret_cast<MpegCallbackData*>(&error));
    }

    return PrintError(message);
}

s32 ErrorValue(MpegSystem* sys, const char* format, s32 value)
{
    char message[0x100];
    snprintf(message, sizeof(message), format, value);
    return Error(sys, message);
}
}

using namespace Libmpeg;

s32 sceMpegCreate(Mpeg* mpeg, u8* work, s32 size)
{
    RetailLibc::MemorySet(work, 0, size);
    MpegSystem* sys = reinterpret_cast<MpegSystem*>((reinterpret_cast<u32>(work) + 3) & ~3u);
    s32 left = size - (reinterpret_cast<u8*>(sys) - work);
    if (left < static_cast<s32>(sizeof(MpegSystem)))
    {
        Error(sys, g_MpegWorkTooSmall);
        return 0;
    }

    mpeg->sys = sys;
    ArenaInitialise(&sys->arena, reinterpret_cast<u8*>(sys + 1), left - sizeof(MpegSystem));
    mpeg->width = 0;
    mpeg->height = 0;
    mpeg->frameCount = 0;
    mpeg->pts = -1;
    mpeg->dts = -1;
    mpeg->flags = 0;
    mpeg->pts2nd = -1;
    mpeg->dts2nd = -1;
    mpeg->flags2nd = 0;
    for (s32 i = 0; i < 3; i++)
    {
        sys->outputCentreHorizontalOffsets[i] = 0;
        sys->outputCentreVerticalOffsets[i] = 0;
    }

    sys->outputDisplayWidth = 0;
    sys->outputDisplayHeight = 0;
    sys->firstStructure = 0;
    sys->output = nullptr;
    sys->outputWidth = 0;
    sys->outputHeight = 0;
    sys->outputMacroblocks = 0;
    sys->forcedBrokenLink = 0;
    sys->pendingPtsState = MpegNoPendingPts;
    sys->callbacks[MpegCallbackError].function = nullptr;
    sys->callbacks[MpegCallbackNoData].function = nullptr;
    sys->callbacks[MpegCallbackBackground].function = nullptr;
    sys->callbacks[MpegCallbackTimeStamp].function = nullptr;
    sys->callbacks[MpegCallbackStream].function = nullptr;
    sys->pendingPts = -1;
    sys->callbacks[MpegCallbackStopDma].function = StopDma;
    sys->callbacks[MpegCallbackRestartDma].function = RestartDma;
    sys->streamCallbacks = reinterpret_cast<MpegStreamCallback*>(
        Allocate(sys, &sys->arena, MpegStreamCallbacks * static_cast<s32>(sizeof(MpegStreamCallback)), 8));
    sys->streamCallbackCount = 0;
    sys->frameBuffers[0] = nullptr;
    sys->frameBuffers[1] = nullptr;
    sys->frameBuffers[2] = nullptr;
    sys->countPts = 0;
    sys->ptsStep = 0;
    sys->lastPts = -1;
    sys->fields = 0;
    sys->ptsRounding = 0;
    sys->firstOutputPicture = 0;
    sys->decodeLimits[MpegIPictures] = -1;
    sys->decodeLimits[MpegPPictures] = -1;
    sys->decodeLimits[MpegBPictures] = -1;
    sys->mpeg = mpeg;
    sys->outputRgb32 = 1;
    SetBuffers(sys);
    sceMpegReset(mpeg);
    sceMpegClearRefBuff(mpeg);
    sys->frames[MpegPast] = &sys->images[0];
    sys->frames[MpegFuture] = &sys->images[1];
    sys->frames[MpegBImage] = &sys->images[2];
    sys->topFields[MpegPast] = &sys->images[3];
    sys->topFields[MpegFuture] = &sys->images[4];
    sys->topFields[MpegBImage] = &sys->images[5];
    sys->bottomFields[MpegPast] = &sys->images[6];
    sys->bottomFields[MpegFuture] = &sys->images[7];
    sys->bottomFields[MpegBImage] = &sys->images[8];
    ArenaMark(&sys->arena);
    sys->temporalReferenceLast = -1;
    sys->temporalReferenceBase = 0;
    sys->gopStarted = 0;
    // Sony's check of its 64 byte aligned buffers, _bstag's and then _idct's (the same bits of the state's address: the second
    // message never comes)
    u32 misalignment = reinterpret_cast<u32>(sys) & (StateAlignment - 1);
    if (misalignment != 0)
    {
        ErrorValue(sys, g_MpegAlignmentError, misalignment);
        return 0;
    }

    return 1;
}

s32 sceMpegInit()
{
    s32 interrupts = DIntr();
    *R_EE_D_ENABLEW = *R_EE_D_ENABLER | DmaSuspend;
    *R_EE_D3_CHCR = *R_EE_D3_CHCR & ~ChcrStart;
    *R_EE_D4_CHCR = *R_EE_D4_CHCR & ~ChcrStart;
    *R_EE_D_ENABLEW = *R_EE_D_ENABLER & ~DmaSuspend;
    if (interrupts != 0)
    {
        EIntr();
    }

    *R_EE_D3_QWC = 0;
    *R_EE_D4_QWC = 0;
    sceIpuInit();
    return 1;
}

s32 sceMpegDelete(Mpeg*)
{
    return 1;
}

s32 sceMpegGetPicture(Mpeg* mpeg, u8* rgb32, s32 macroblocks)
{
    MpegSystem* sys = mpeg->sys;
    sys->outputRgb32 = 1;
    sys->output = reinterpret_cast<u8*>((reinterpret_cast<u32>(rgb32) & PhysicalMask) | UncachedSegment);
    sys->outputMacroblocks = macroblocks;
    sys->outputWidth = 0;
    sys->outputHeight = 0;
    return GetPicture(mpeg);
}

s32 sceMpegGetPictureRAW8(Mpeg* mpeg, u8* raw8, s32 macroblocks)
{
    MpegSystem* sys = mpeg->sys;
    sys->outputMacroblocks = macroblocks;
    sys->output = reinterpret_cast<u8*>((reinterpret_cast<u32>(raw8) & PhysicalMask) | UncachedSegment);
    sys->outputWidth = 0;
    sys->outputRgb32 = 0;
    sys->outputHeight = 0;
    return GetPicture(mpeg);
}

void MpegStreamEnded(Mpeg* mpeg)
{
    mpeg->sys->aborted = 1;
}

s32 sceMpegReset(Mpeg* mpeg)
{
    MpegSystem* sys = mpeg->sys;
    sys->aborted = 0;
    sys->ended = 0;
    sys->pictureWaiting = 0;
    sys->outputState = MpegNothingDecoded;
    mpeg->frameCount = 0;
    sys->firstOutputPicture = 0;
    sys->lastPts = -1;
    StopDecoding(sys);
    sys->mpeg2 = 0;
    sys->pictureNumber = 0;
    IpuControlRegister control = {*IpuControl};
    control.mpeg1 = 1;
    *IpuControl = control.value;
    return 1;
}

s32 sceMpegIsEnd(Mpeg* mpeg)
{
    return mpeg->sys->ended;
}

void* sceMpegAddCallback(Mpeg* mpeg, s32 type, MpegCallback callback, void* user)
{
    MpegCallbackEntry* entry = &mpeg->sys->callbacks[type];
    MpegCallback previous = entry->function;
    entry->function = callback;
    entry->user = user;
    entry->globalPointer = GlobalPointer();
    return reinterpret_cast<void*>(previous);
}

s32 sceMpegClearRefBuff(Mpeg* mpeg)
{
    MpegSystem* sys = mpeg->sys;
    if (sys->frames[MpegPast] != nullptr)
    {
        sys->frames[MpegPast]->holdsPicture = 0;
    }

    if (sys->topFields[MpegPast] != nullptr)
    {
        sys->topFields[MpegPast]->holdsPicture = 0;
    }

    if (sys->bottomFields[MpegPast] != nullptr)
    {
        sys->bottomFields[MpegPast]->holdsPicture = 0;
    }

    if (sys->frames[MpegFuture] != nullptr)
    {
        sys->frames[MpegFuture]->holdsPicture = 0;
    }

    if (sys->topFields[MpegFuture] != nullptr)
    {
        sys->topFields[MpegFuture]->holdsPicture = 0;
    }

    if (sys->bottomFields[MpegFuture] != nullptr)
    {
        sys->bottomFields[MpegFuture]->holdsPicture = 0;
    }

    return 1;
}
