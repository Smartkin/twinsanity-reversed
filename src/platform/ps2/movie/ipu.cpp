#include "mpeg.h"

// Sony's libipu: the IPU's set-up, its waits, and its DMA stopped and started again around another use of the IPU

namespace
{
// A quantiser matrix: 64 bytes, 4 quadwords
constexpr u32 MatrixBytes = 0x40;
constexpr s32 MatrixQuadwords = MatrixBytes / QuadwordBytes;
}

extern "C"
{
    // The default intra quantiser matrix, then a row of the flat non-intra one (16s)
    extern const u8 g_IpuDefaultMatrices[MatrixBytes + QuadwordBytes] RETAIL(D_002E8310);
    // The VQ colour table (16 colours of 16 bits)
    extern const u8 g_IpuVqClut[2 * QuadwordBytes] RETAIL(D_002E8360);
}

namespace
{
volatile u32* const IpuInput = reinterpret_cast<volatile u32*>(A_EE_IPU_in_FIFO);

// A quadword into the IPU's input FIFO (one 128 bit store)
void SendQuadword(const u8* quadword)
{
    asm volatile("lq $8, 0(%1)\n\t"
                 "sq $8, 0(%0)"
                 :
                 : "r"(IpuInput), "r"(quadword)
                 : "$8", "memory");
}

void SetChcr(volatile u32* chcr, u32 value)
{
    s32 interrupts = DIntr();
    *R_EE_D_ENABLEW = *R_EE_D_ENABLER | DmaSuspend;
    *chcr = value;
    *R_EE_D_ENABLEW = *R_EE_D_ENABLER & ~DmaSuspend;
    if (interrupts != 0)
    {
        EIntr();
    }
}
}

namespace Libipu
{
void SetFromIpuChcr(u32 chcr)
{
    SetChcr(R_EE_D3_CHCR, chcr);
}

void SetToIpuChcr(u32 chcr)
{
    SetChcr(R_EE_D4_CHCR, chcr);
}

void SetToIpuChcrForInit(u32 chcr)
{
    SetChcr(R_EE_D4_CHCR, chcr);
}
}

using namespace Libipu;

void sceIpuStopDMA(IpuDmaEnvironment* environment)
{
    SetToIpuChcr(ChcrFromMemory);
    environment->toMadr = *R_EE_D4_MADR;
    environment->toTadr = *R_EE_D4_TADR;
    environment->toQwc = *R_EE_D4_QWC;
    environment->toChcr = *R_EE_D4_CHCR;
    while (IpuControlRegister{*IpuControl}.outputQuadwords != 0)
    {
    }

    SetFromIpuChcr(0);
    environment->fromMadr = *R_EE_D3_MADR;
    environment->fromQwc = *R_EE_D3_QWC;
    environment->fromChcr = *R_EE_D3_CHCR;
    environment->bitPosition.value = *IpuBitPosition;
    environment->control.value = *IpuControl;
}

void sceIpuRestartDMA(IpuDmaEnvironment* environment)
{
    // What the toIPU channel sent and the IPU didn't read yet goes again: the input FIFO's quadwords and the ones the IPU took to
    // read, from the bit position in them
    IpuBitPositionRegister position = environment->bitPosition;
    u32 unread = position.heldQuadwords + position.fifoQuadwords;
    u32 toQwc = environment->toQwc + unread;
    u32 toMadr = environment->toMadr - unread * QuadwordBytes;
    if (environment->fromMadr != 0 && environment->fromQwc != 0)
    {
        *R_EE_D3_MADR = environment->fromMadr;
        *R_EE_D3_QWC = environment->fromQwc;
        SetFromIpuChcr(environment->fromChcr | ChcrStart);
    }

    WaitIpu();
    *IpuCommand = IpuClearInput | position.bitPosition;
    WaitIpu();
    if (toMadr != 0 && toQwc != 0)
    {
        *R_EE_D4_MADR = toMadr;
        *R_EE_D4_TADR = environment->toTadr;
        *R_EE_D4_QWC = toQwc;
        SetToIpuChcr(environment->toChcr | ChcrStart);
    }
}

s32 sceIpuSync(s32 mode, u16)
{
    if (mode == IpuSyncWait)
    {
        WaitIpu();
        return 0;
    }

    if (mode == IpuSyncPoll)
    {
        return IpuControlRegister{*IpuControl}.busy;
    }

    return 0;
}

void sceIpuInit()
{
    SetToIpuChcrForInit(ChcrFromMemory);
    *IpuControl = IpuControlReset;
    WaitIpu();
    *IpuCommand = IpuClearInput;
    WaitIpu();
    // The intra matrix, then the non-intra one: the same row 4 times
    for (s32 i = 0; i < MatrixQuadwords; i++)
    {
        SendQuadword(&g_IpuDefaultMatrices[i * QuadwordBytes]);
    }

    for (s32 i = 0; i < MatrixQuadwords; i++)
    {
        SendQuadword(&g_IpuDefaultMatrices[MatrixBytes]);
    }

    *IpuCommand = IpuSetIntraMatrix;
    WaitIpu();
    *IpuCommand = IpuSetNonIntraMatrix;
    WaitIpu();
    SendQuadword(&g_IpuVqClut[0]);
    SendQuadword(&g_IpuVqClut[QuadwordBytes]);
    *IpuCommand = IpuSetVqClut;
    WaitIpu();
    *IpuCommand = IpuSetThresholds;
    WaitIpu();
    *IpuControl = IpuControlReset;
    WaitIpu();
    *IpuCommand = IpuClearInput;
    WaitIpu();
}
