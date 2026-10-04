#include "mpeg.h"

// libmpeg's bit reader: the IPU's input stream read through its commands

namespace Libmpeg
{
namespace
{
// The bits the IPU's input FIFO holds: its quadwords and the one being read, less those read of the first
u32 BitsInFifo(u32 bitPosition)
{
    return ((bitPosition & 0xFF00) >> 1) + ((bitPosition & 0x30000) >> 9) - (bitPosition & 0x7F);
}

bool ReadsBits(u32 code)
{
    return code == IpuDecodeBlock || code == IpuDecodeVariable || code == IpuDecodeFixed;
}

// While a command waits: the NoData callback when a decoding command lacks bits and the toIPU channel is idle, and again every
// pollLimit polls. Returns false when the decoding was aborted (the IPU's DMA then stopped)
bool FeedIpu(MpegSystem* sys, s32& polls, s32 pollLimit)
{
    u32 bitPosition = *IpuBitPosition;
    if (ReadsBits(sys->ipuCommand) && BitsInFifo(bitPosition) < 32 && *R_EE_D4_QWC == 0)
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
    if ((*IpuControl & (IpuControlBusy | IpuControlErrorCode)) == IpuControlBusy)
    {
        s32 polls = 0;
        do
        {
            if (!FeedIpu(sys, polls, 5000))
            {
                break;
            }
        } while ((*IpuControl & (IpuControlBusy | IpuControlErrorCode)) == IpuControlBusy);
    }

    sys->ipuCommand = 0;
}

u64 WaitIpuResult(MpegSystem* sys)
{
    u64 result = *R_EE_IPU_CMD;
    if (static_cast<s64>(result) < 0 && (*IpuControl & IpuControlErrorCode) == 0)
    {
        s32 polls = 0;
        do
        {
            if (!FeedIpu(sys, polls, 500))
            {
                break;
            }

            result = *R_EE_IPU_CMD;
        } while (static_cast<s64>(result) < 0 && (*IpuControl & IpuControlErrorCode) == 0);
    }

    sys->ipuCommand = 0;
    return result;
}

void WaitIpuIdleIfBusy(MpegSystem* sys)
{
    if ((*IpuControl & (IpuControlBusy | IpuControlErrorCode)) == IpuControlBusy)
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

    sys->topBits = 32;
    u32 bits = sys->top >> ((32 - count) & 0x1F);
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
        sys->topBits = 32;
    }

    return sys->top >> (-count & 0x1F);
}

void SkipBits(MpegSystem* sys, s32 count)
{
    WaitIpuIdleIfBusy(sys);
    SetIpuCommand(sys, IpuDecodeFixed | count);
    sys->top = WaitIpuResult(sys);
    sys->topBits = 32;
}

void SkipMacroblockBits(MpegSystem* sys, s32 count)
{
    SkipBits(sys, count);
}
}
