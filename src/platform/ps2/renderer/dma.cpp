#include "renderer.h"

#include <ee_regs.h>

// The renderer's DMA channels: a chain sent on a channel once it's done with the one before, whether a channel is sending, and
// the waits until the chain the renderer sent on a channel is sent

namespace
{
// The channels' registers (SetDmaRegisterPointers' table): CHCR, with QWC and TADR after it
constexpr u32 ChannelRegisters[] = {A_EE_D0_CHCR, A_EE_D1_CHCR, A_EE_D2_CHCR, A_EE_D3_CHCR, A_EE_D4_CHCR,
                                    A_EE_D5_CHCR, A_EE_D6_CHCR, A_EE_D7_CHCR, A_EE_D8_CHCR, A_EE_D9_CHCR};
constexpr u32 QwcOffset = A_EE_D0_QWC - A_EE_D0_CHCR;
constexpr u32 TadrOffset = A_EE_D0_TADR - A_EE_D0_CHCR;
// D_CHCR: DIR from memory, MOD chain, STR, and TTE (the tags sent too)
constexpr u32 ChainFromMemory = 0x105;
constexpr u32 ChainWithTags = 0x145;
// TADR takes the scratchpad's addresses (0x70000000 up) with SPR (bit 31) set, main memory's physical ones
constexpr u32 ScratchpadAddress = 0x70000000;
constexpr u32 ScratchpadBit = 0x80000000;
constexpr u32 PhysicalMask = 0x0FFFFFFF;
// D_PCR.CDE for every channel: COP0's condition follows them
constexpr u32 ConditionChannels = 0x3FF0000;
// The pause between two looks at a busy channel
constexpr s32 BusyPause = 100;
constexpr s32 Vif1Channel = 1;

volatile u32* ChannelRegister(s32 channel, u32 offset)
{
    return reinterpret_cast<volatile u32*>(ChannelRegisters[channel] + offset);
}

// CHCR's STR, read as its byte
bool IsSending(volatile u32* control)
{
    return (reinterpret_cast<volatile u8*>(control)[1] & 1) != 0;
}
}

bool IsDmaChannelBusy(s32 channel)
{
    return IsSending(ChannelRegister(channel, 0));
}

void StartDmaChain(s32 channel, const void* chain, bool sendTags)
{
    volatile u32* control = ChannelRegister(channel, 0);
    while (IsSending(control))
    {
        for (s32 pause = 0; pause < BusyPause; pause++)
        {
            asm volatile("nop");
        }
    }

    *ChannelRegister(channel, QwcOffset) = 0;
    u32 address = reinterpret_cast<u32>(chain);
    if ((address & ScratchpadAddress) == ScratchpadAddress)
    {
        address = (address & PhysicalMask) | ScratchpadBit;
    }
    else
    {
        address &= PhysicalMask;
    }

    *ChannelRegister(channel, TadrOffset) = address;
    asm volatile("sync" : : : "memory");
    *control = sendTags ? ChainWithTags : ChainFromMemory;
}

s32 WaitForDmaChannel(s32 channel)
{
    RendererDmaChannel& dma = g_RendererDma[channel];
    while (dma.sending != 0)
    {
        *R_EE_D_PCR = dma.statusBit | ConditionChannels;
        asm volatile(".set push\n"
                     ".set noreorder\n"
                     "1:\n"
                     "nop\n"
                     "nop\n"
                     "nop\n"
                     "nop\n"
                     "bc0f 1b\n"
                     "nop\n"
                     ".set pop\n"
                     :
                     :
                     : "memory");
        if ((*R_EE_D_STAT & dma.statusBit) != 0)
        {
            dma.sending = 0;
            *R_EE_D_STAT = dma.statusBit;
        }
    }

    return 1;
}

s32 WaitForVif1Dma()
{
    return WaitForDmaChannel(Vif1Channel);
}

extern "C"
{
    // libdma's channels' registers (graphics.cpp)
    volatile u32* sceDmaGetChan(u32 channel) RETAIL(sceDmaGetChan);
    // The register each channel's address goes in (TADR for the channels that follow chains, MADR for the others), which nothing
    // reads
    extern volatile u32* g_DmaAddressRegisters[10] RETAIL(VIF0_TADR_PTR);
}

namespace
{
constexpr u32 AddressRegisters[] = {A_EE_D0_TADR, A_EE_D1_TADR, A_EE_D2_TADR, A_EE_D3_MADR, A_EE_D4_TADR,
                                    A_EE_D5_MADR, A_EE_D6_TADR, A_EE_D7_MADR, A_EE_D8_MADR, A_EE_D9_TADR};
constexpr s32 DmaChannels = 10;
// D_PCR: PCE, and CDE for every channel
constexpr u32 PriorityAndChannels = 0x83FF0000;
// The SIF's channels (5 to 7), which the renderer leaves alone
constexpr u32 FirstSifChannel = 5;
constexpr u32 SifChannels = 3;
// D_CHCR.TTE: the tags are sent with the data
constexpr u32 TagsSent = 0x40;
}

void SetDmaRegisterPointers()
{
    *R_EE_D_PCR = PriorityAndChannels;
    for (s32 channel = 0; channel < DmaChannels; channel++)
    {
        g_DmaAddressRegisters[channel] = reinterpret_cast<volatile u32*>(AddressRegisters[channel]);
    }

    for (u32 channel = 0; channel < DmaChannels; channel++)
    {
        if (channel - FirstSifChannel < SifChannels)
        {
            continue;
        }

        RendererDmaChannel& dma = g_RendererDma[channel];
        dma.registers = sceDmaGetChan(channel);
        *dma.registers |= TagsSent;
        dma.statusBit = 1 << channel;
        dma.sending = 0;
    }
}

s32 FinishDMATransferAll()
{
    for (s32 channel = 0; channel < DmaChannels; channel++)
    {
        WaitForDmaChannel(channel);
    }

    return 1;
}
