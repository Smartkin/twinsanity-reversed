#include "renderer.h"

#include <ee_regs.h>

// The renderer's DMA channels: a chain sent on a channel once it's done with the one before, whether a channel is sending, and
// the waits until the chain the renderer sent on a channel is sent

namespace
{
// The channels' registers (SetDmaRegisterPointers' table): CHCR, with QWC and TADR after it
constexpr u32 ChannelRegisters[] = {A_EE_D0_CHCR, A_EE_D1_CHCR, A_EE_D2_CHCR, A_EE_D3_CHCR, A_EE_D4_CHCR,
                                    A_EE_D5_CHCR, A_EE_D6_CHCR, A_EE_D7_CHCR, A_EE_D8_CHCR, A_EE_D9_CHCR};
// The pause between two looks at a busy channel
constexpr s32 BusyPause = 100;

volatile u32* ChannelRegister(s32 channel, u32 offset)
{
    return reinterpret_cast<volatile u32*>(ChannelRegisters[channel] + offset);
}

// CHCR's STR (bit 8), read as its byte
bool IsSending(volatile u32* control)
{
    constexpr u32 StartedByte = 1;
    constexpr u8 StartedBit = 1;
    return (reinterpret_cast<volatile u8*>(control)[StartedByte] & StartedBit) != 0;
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
    if ((address & Scratchpad) == Scratchpad)
    {
        address = (address & PhysicalMask) | DmaScratchpad;
    }
    else
    {
        address &= PhysicalMask;
    }

    *ChannelRegister(channel, TadrOffset) = address;
    asm volatile("sync" : : : "memory");
    // From memory in chain mode, the tags sent too when asked
    DmaChannelControl start = {};
    start.fromMemory = 1;
    start.mode = DmaChainMode;
    start.sendsTags = sendTags;
    start.started = 1;
    *control = start.value;
}

s32 WaitForDmaChannel(s32 channel)
{
    RendererDmaChannel& dma = g_RendererDma[channel];
    while (dma.sending != 0)
    {
        *R_EE_D_PCR = WaitOnChannels(dma.statusBit);
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
    extern volatile u32* g_DmaAddressRegisters[DmaChannels] RETAIL(VIF0_TADR_PTR);
}

namespace
{
constexpr u32 AddressRegisters[] = {A_EE_D0_TADR, A_EE_D1_TADR, A_EE_D2_TADR, A_EE_D3_MADR, A_EE_D4_TADR,
                                    A_EE_D5_MADR, A_EE_D6_TADR, A_EE_D7_MADR, A_EE_D8_MADR, A_EE_D9_TADR};
// The SIF's channels (5 to 7), which the renderer leaves alone
constexpr u32 FirstSifChannel = 5;
constexpr u32 SifChannels = 3;
}

// Priority control on, every channel enabled
void SetDmaRegisterPointers()
{
    DmaPriorityControl priority = {};
    priority.enabledChannels = AllDmaChannels;
    priority.priorityControl = 1;
    *R_EE_D_PCR = priority.value;
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
        // The tags sent with the data (TTE)
        DmaChannelControl control;
        control.value = *dma.registers;
        control.sendsTags = 1;
        *dma.registers = control.value;
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
