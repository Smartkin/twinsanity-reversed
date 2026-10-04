#include "mpeg.h"

// Sony's libipu: the IPU's set-up, its waits, and its DMA stopped and started again around another use of the IPU

extern "C"
{
    // The default intra quantiser matrix, then a row of the flat non-intra one (16s)
    extern const u8 g_IpuDefaultMatrices[0x50] RETAIL(D_002E8310);
    // The VQ colour table (16 colours of 16 bits)
    extern const u8 g_IpuVqClut[0x20] RETAIL(D_002E8360);
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
    while ((*IpuControl & IpuControlOutputCount) != 0)
    {
    }

    SetFromIpuChcr(0);
    environment->fromMadr = *R_EE_D3_MADR;
    environment->fromQwc = *R_EE_D3_QWC;
    environment->fromChcr = *R_EE_D3_CHCR;
    environment->bitPosition = *IpuBitPosition;
    environment->control = *IpuControl;
}

void sceIpuRestartDMA(IpuDmaEnvironment* environment)
{
    // IPU_BP: the bit position, the quadwords in the input FIFO and FP (the quadword being read): what the toIPU channel sent and
    // the IPU didn't read yet goes again
    u32 bitPosition = environment->bitPosition;
    u32 unread = ((bitPosition >> 16) & 0x3) + ((bitPosition >> 8) & 0xF);
    u32 toQwc = environment->toQwc + unread;
    u32 toMadr = environment->toMadr - (unread << 4);
    if (environment->fromMadr != 0 && environment->fromQwc != 0)
    {
        *R_EE_D3_MADR = environment->fromMadr;
        *R_EE_D3_QWC = environment->fromQwc;
        SetFromIpuChcr(environment->fromChcr | ChcrStart);
    }

    WaitIpu();
    *IpuCommand = IpuClearInput | (bitPosition & 0x7F);
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
    if (mode == 0)
    {
        WaitIpu();
        return 0;
    }

    if (mode == 1)
    {
        return *IpuControl >> 31;
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
    for (s32 i = 0; i < 4; i++)
    {
        SendQuadword(&g_IpuDefaultMatrices[i * 0x10]);
    }

    for (s32 i = 0; i < 4; i++)
    {
        SendQuadword(&g_IpuDefaultMatrices[0x40]);
    }

    *IpuCommand = IpuSetIntraMatrix;
    WaitIpu();
    *IpuCommand = IpuSetNonIntraMatrix;
    WaitIpu();
    SendQuadword(&g_IpuVqClut[0]);
    SendQuadword(&g_IpuVqClut[0x10]);
    *IpuCommand = IpuSetVqClut;
    WaitIpu();
    *IpuCommand = IpuSetThresholds;
    WaitIpu();
    *IpuControl = IpuControlReset;
    WaitIpu();
    *IpuCommand = IpuClearInput;
    WaitIpu();
}
