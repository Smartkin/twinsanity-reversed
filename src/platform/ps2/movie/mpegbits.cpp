#include "mpeg.h"

// libmpeg's bit reader: the IPU's input stream read through its commands

namespace Libmpeg
{
namespace
{
constexpr u32 QuadwordBits = 128;
// The bits read ahead into top: a word
constexpr s32 TopBits = 32;
// The polls of a wait between two NoData callbacks: for the IPU to be done, and for a command's result
constexpr s32 IdlePolls = 5000;
constexpr s32 ResultPolls = 500;

// The bits the IPU's input holds: the FIFO's quadwords and the ones the IPU took to read, less those read of the first
u32 BitsInFifo(IpuBitPositionRegister position)
{
    return position.fifoQuadwords * QuadwordBits + position.heldQuadwords * QuadwordBits - position.bitPosition;
}

bool ReadsBits(u32 code)
{
    return code == IpuDecodeBlock || code == IpuDecodeVariable || code == IpuDecodeFixed;
}

// While a command waits: the NoData callback when a decoding command lacks bits and the toIPU channel is idle, and again every
// pollLimit polls. Returns false when the decoding was aborted (the IPU's DMA then stopped)
bool FeedIpu(MpegSystem* sys, s32& polls, s32 pollLimit)
{
    IpuBitPositionRegister position = {*IpuBitPosition};
    if (ReadsBits(sys->ipuCommand) && BitsInFifo(position) < static_cast<u32>(TopBits) && *R_EE_D4_QWC == 0)
    {
        DispatchNoData(sys->mpeg);
        if (sys->aborted != 0)
        {
            StopIpuDma(sys);
            return false;
        }

        polls = 0;
    }

    if (polls++ > pollLimit)
    {
        DispatchNoData(sys->mpeg);
        polls = 0;
        if (sys->aborted != 0)
        {
            StopIpuDma(sys);
            return false;
        }
    }

    return true;
}
}

void WaitIpuIdle(MpegSystem* sys)
{
    if (IpuWorking())
    {
        s32 polls = 0;
        do
        {
            if (!FeedIpu(sys, polls, IdlePolls))
            {
                break;
            }
        } while (IpuWorking());
    }

    sys->ipuCommand = 0;
}

u64 WaitIpuResult(MpegSystem* sys)
{
    IpuDataRegister result = {*R_EE_IPU_CMD};
    if (result.busy && !IpuControlRegister{*IpuControl}.errorFound)
    {
        s32 polls = 0;
        do
        {
            if (!FeedIpu(sys, polls, ResultPolls))
            {
                break;
            }

            result.value = *R_EE_IPU_CMD;
        } while (result.busy && !IpuControlRegister{*IpuControl}.errorFound);
    }

    sys->ipuCommand = 0;
    return result.value;
}

void WaitIpuIdleIfBusy(MpegSystem* sys)
{
    if (IpuWorking())
    {
        WaitIpuIdle(sys);
    }
}

u32 NextBits(MpegSystem* sys, s32 count)
{
    WaitIpuIdleIfBusy(sys);
    if (sys->bitsStale != 0 || sys->topBits < count)
    {
        *IpuCommand = IpuDecodeFixed;
        sys->bitsStale = 0;
        sys->ipuCommand = IpuDecodeFixed;
        sys->top = WaitIpuResult(sys);
    }

    sys->topBits = TopBits;
    u32 bits = sys->top >> ((TopBits - count) & (TopBits - 1));
    SetIpuCommand(sys, IpuDecodeFixed | count);
    sys->top = WaitIpuResult(sys);
    return bits;
}

u32 PeekBits(MpegSystem* sys, s32 count)
{
    if (sys->bitsStale != 0 || sys->topBits < count)
    {
        WaitIpuIdleIfBusy(sys);
        *IpuCommand = IpuDecodeFixed;
        sys->bitsStale = 0;
        sys->ipuCommand = IpuDecodeFixed;
        sys->top = WaitIpuResult(sys);
        sys->topBits = TopBits;
    }

    return sys->top >> (-count & (TopBits - 1));
}

void SkipBits(MpegSystem* sys, s32 count)
{
    WaitIpuIdleIfBusy(sys);
    SetIpuCommand(sys, IpuDecodeFixed | count);
    sys->top = WaitIpuResult(sys);
    sys->topBits = TopBits;
}

void SkipMacroblockBits(MpegSystem* sys, s32 count)
{
    SkipBits(sys, count);
}
}
