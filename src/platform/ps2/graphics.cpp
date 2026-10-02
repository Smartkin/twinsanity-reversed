#include "platform/graphics.h"

#include "renderer/renderer.h"

#include <ee_regs.h>
#include <kernel.h>
#include <gif_registers.h>
#include <vif_registers.h>

// Sony's libgraph, still in the asm
extern "C" int sceGsSyncV(int mode) asm("RenderVSync");

// The resets of Sony's libdev, libgraph and libdma the game starts with, on PS2SDK's register definitions. PS2SDK's own (ResetEE,
// dma_reset) reset the whole DMAC: this one leaves the SIF's channels (5 to 7), which the IOP's communication already runs on
namespace
{
constexpr u32 VifReset = 0x1;           // FBRST.RST
constexpr u32 VifMaskAllErrors = 0x6;   // ERR.ME0 and ME1: a DMA tag mismatch or a bad VIF code doesn't stop the VIF
constexpr u32 VifMaskTagMismatch = 0x2; // ERR.ME0
constexpr u32 Vu0Reset = 0x2;           // FBRST.RS0
constexpr u32 Vu1Reset = 0x200;         // FBRST.RS1
constexpr u32 VuDebugBreaks = 0x404;    // FBRST.DE0 and DE1
constexpr u32 Vu1Busy = 0x100;          // VPU-STAT.VBS1

// Channels 0-4, 8 and 9: VIF0, VIF1, GIF, IPU from and to, SPR from and to
constexpr u32 ResetChannels[] = {0x10008000, 0x10009000, 0x1000A000, 0x1000B000, 0x1000B400, 0x1000D000, 0x1000D400};
// Their status and mask bits in D_STAT, and the stall, MFIFO and cycle stealing settings of D_CTRL
constexpr u32 ResetChannelStatus = 0xFF1F;
constexpr u32 ResetChannelMasks = 0xFF1F0000;
constexpr u32 DmacKeptSettings = 0xFFFFFF01;
constexpr u32 DmacEnable = 0x1;

// VIF1's settings after a reset: STCYCL 4/4, STMASK 0, STMOD 0, MSKPATH3 off, BASE, OFFSET and ITOP 0
alignas(16) const u32 Vif1Settings[8] = {0x01000404, 0x20000000, 0, 0x05000000, 0x06000000, 0x03000000, 0x02000000, 0x04000000};

u32 ReadVuFbrst()
{
    u32 value;
    asm volatile("cfc2.ni %0, $vi28" : "=r"(value));
    return value;
}

void WriteVuFbrst(u32 value)
{
    asm volatile("ctc2.ni %0, $vi28" : : "r"(value));
}

u32 ReadVpuStat()
{
    u32 value;
    asm volatile("cfc2.ni %0, $vi29" : "=r"(value));
    return value;
}

void WriteQuadword(u32 address, const u32* value)
{
    asm volatile("lq $8, 0(%0)\n\tsq $8, 0(%1)" : : "r"(value), "r"(address) : "$8", "memory");
}

void ResetVif0()
{
    VIF0_FBRST = VifReset;
    VIF0_ERR = VifMaskAllErrors;
}

void ResetVif1()
{
    VIF1_FBRST = VifReset;
    VIF1_ERR = VifMaskAllErrors;
}

void ResetVu0()
{
    WriteVuFbrst(ReadVuFbrst() | Vu0Reset);
}

void ResetVu1()
{
    WriteVuFbrst(ReadVuFbrst() | Vu1Reset);
}

// sceDmaReset(1), which enables it
void ResetDmac()
{
    for (u32 base : ResetChannels)
    {
        volatile u32* channel = reinterpret_cast<volatile u32*>(base);
        channel[0x80 / 4] = 0; // SADR
        channel[0x00 / 4] = 0; // CHCR
        channel[0x30 / 4] = 0; // TADR
        channel[0x10 / 4] = 0; // MADR
        channel[0x50 / 4] = 0; // ASR1
        channel[0x40 / 4] = 0; // ASR0
    }

    // Writing a status bit clears it, writing a mask bit flips it
    *R_EE_D_STAT = ResetChannelStatus;
    *R_EE_D_STAT = *R_EE_D_STAT & ResetChannelMasks;
    *R_EE_D_CTRL = *R_EE_D_CTRL & DmacKeptSettings;
    *R_EE_D_PCR = 0;
    *R_EE_D_SQWC = 0;
    *R_EE_D_RBOR = 0;
    *R_EE_D_RBSR = 0;
    *R_EE_D_CTRL = *R_EE_D_CTRL | DmacEnable;
}
}

// VIF1, VU1 and the GIF: the GS's path 1 and 2 (sceGsResetPath)
void Platform::Graphics::ResetPath()
{
    VIF1_FBRST = VifReset;
    VIF1_ERR = VifMaskTagMismatch;
    asm volatile("sync");
    WriteVuFbrst(ReadVuFbrst() | Vu1Reset);
    asm volatile("sync.p");
    while (ReadVpuStat() & Vu1Busy)
    {
    }

    WriteVuFbrst(VuDebugBreaks);
    WriteQuadword(VIF1_FIFO, Vif1Settings);
    WriteQuadword(VIF1_FIFO, Vif1Settings + 4);
    GIF_REG_CTRL = 1;
}

void Platform::Graphics::ResetDevices()
{
    ResetVif0();
    ResetVif1();
    ResetVu0();
    ResetVu1();
    ResetPath();
    ResetDmac();
}

void Platform::Graphics::WaitVSync()
{
    sceGsSyncV(0);
}

extern "C"
{
    // The GS's video mode the renderer set the display up in (sceGsResetGraph's: 2 NTSC, 3 PAL)
    extern s32 g_VideoOutMode RETAIL(G_VideoOutMode);
}

bool Platform::Graphics::IsPalDisplay()
{
    constexpr s32 Pal = 3;
    return g_VideoOutMode == Pal;
}

extern "C"
{
    // The renderer's waits for its DMA channels (still asm) and libgraph's for the path to the GS
    void FinishDMATransferChannel1();
    void FinishDMATransferAll();
    void WaitGraphicalDataTransferFinish(s32 mode, s32 timeout);

    // The display's settings the renderer worked out at start-up (FUN_0019b570): the GS's PCRTC registers' fields
    struct DisplaySettings
    {
        // DISPFB: the frame buffer's base (in 2048 word pages), width (in 64 pixels) and pixel format
        u32 frameBuffer;
        u32 frameWidth;
        u32 pixelFormat;
        u32 unknown0C;
        u32 unknown10;
        u32 unknown14;
        u32 unknown18;
        // PMODE
        u32 alpha;
        u32 enable1;
        u32 enable2;
        u32 alphaFromRegister;
        u32 blendWithBackground;
        u32 crtMode;
        u32 alphaOutput;
        u32 unknown38;
        u32 unknown3C;
        // DISPLAY: the magnifications, the size (minus 1 for the height) and the offset from the TV's corner
        u32 magnifyX;
        u32 magnifyY;
        u32 width;
        u32 height;
        s32 x;
        s32 y;
    };
    extern DisplaySettings g_DisplaySettings RETAIL(G_FrameBufferBasePointer);
}

namespace
{
constexpr u32 GifChannel = 2;
constexpr u32 ChainFromMemory = 0x105; // D_CHCR: DIR from memory, MOD chain, STR
constexpr u32 ConditionChannels = 0x3FF0000; // D_PCR.CDE for every channel: COP0's condition follows them

// The first frame keeps the display the reset set up (the retail G_IsPCRTC_Ready starts at 1)
bool g_SkipDisplaySetUp = true;

// The GS's PCRTC: the circuits' settings from the renderer's (SetupPCRTC)
void SetUpDisplay()
{
    const DisplaySettings& display = g_DisplaySettings;
    u64 mode = display.enable1 | static_cast<u64>(display.enable2) << 1 | static_cast<u64>(display.crtMode) << 2 |
               static_cast<u64>(display.alphaFromRegister) << 5 | static_cast<u64>(display.alphaOutput) << 6 |
               static_cast<u64>(display.blendWithBackground) << 7 | static_cast<u64>(display.alpha) << 8;
    *reinterpret_cast<volatile u64*>(0x12000000) = mode;
    // SMODE2: interlaced, a field at a time
    *reinterpret_cast<volatile u64*>(0x12000020) = 1;
    u64 area = static_cast<u64>(display.magnifyX) << 23 | static_cast<u64>(display.magnifyY) << 27 |
               static_cast<u64>(display.width) << 32 | static_cast<u64>(display.height) << 44;
    u64 x = static_cast<u32>(display.x + 0x27C);
    // The second circuit a line lower
    *reinterpret_cast<volatile u64*>(0x12000080) = x | static_cast<u64>(static_cast<u32>(display.y + 0x32)) << 12 | area;
    *reinterpret_cast<volatile u64*>(0x120000A0) = x | static_cast<u64>(static_cast<u32>(display.y + 0x33)) << 12 | area;
    u64 frame = display.frameBuffer | static_cast<u64>(display.frameWidth) << 9 | static_cast<u64>(display.pixelFormat) << 15;
    *reinterpret_cast<volatile u64*>(0x12000070) = frame;
    *reinterpret_cast<volatile u64*>(0x12000090) = frame;
}

// Starts sending the chain to the GIF (Start_DMAC_GIF_Transfer, FUN_00182178 from an interrupt)
void SendToGif(const void* chain, bool fromInterrupt)
{
    if (fromInterrupt)
    {
        iFlushCache(0);
    }
    else
    {
        FlushCache(0);
    }

    RendererDmaChannel& gif = g_RendererDma[GifChannel];
    *R_EE_D_STAT = gif.statusBit;
    gif.sending = 1;
    *R_EE_D2_QWC = 0;
    *R_EE_D2_TADR = reinterpret_cast<u32>(chain) & 0x0FFFFFFF;
    *R_EE_D2_CHCR = ChainFromMemory;
    asm volatile("sync" : : : "memory");
}

void SetUpDisplayUnlessSkipped()
{
    if (g_SkipDisplaySetUp)
    {
        g_SkipDisplaySetUp = false;
    }
    else
    {
        SetUpDisplay();
    }
}
}

void Platform::Graphics::WaitIdle()
{
    FlushCache(0);
    FinishDMATransferChannel1();
    FinishDMATransferAll();
    WaitGraphicalDataTransferFinish(0, 0);
}

// PerformRender
void Platform::Graphics::Present(const void* commands)
{
    WaitIdle();
    // Timer 1 counts the frame again
    *R_EE_T1_MODE = 0;
    sceGsSyncV(0);
    if (commands != nullptr)
    {
        SendToGif(commands, false);
    }

    SetUpDisplayUnlessSkipped();
}

// FUN_001a0860
void Platform::Graphics::PresentFromInterrupt(const void* commands)
{
    iFlushCache(0);
    FinishDMATransferChannel1();
    FinishDMATransferAll();
    WaitGraphicalDataTransferFinish(0, 0);
    *R_EE_T1_MODE = 0;
    SendToGif(commands, true);
    SetUpDisplayUnlessSkipped();
}

// FUN_00182200: the channel's status bit is waited for through COP0's condition, which D_PCR makes follow the channel
void Platform::Graphics::WaitSent()
{
    RendererDmaChannel& gif = g_RendererDma[GifChannel];
    while (gif.sending != 0)
    {
        u32 bit = gif.statusBit;
        *R_EE_D_PCR = bit | ConditionChannels;
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
        if ((*R_EE_D_STAT & bit) != 0)
        {
            *R_EE_D_STAT = bit;
            gif.sending = 0;
        }
    }

    WaitGraphicalDataTransferFinish(0, 0);
}

extern "C"
{
    // The VU0 microcode sets and the one loaded (G_UnkDmaRelated), switched by a DMA to VIF0 (still asm)
    extern u8 g_Vu0Programs[] RETAIL(G_UnkDmaRelated);
    void SelectVu0Programs(u8* programs, u32 set, bool wait) RETAIL(FUN_002b20e0);
}

void Platform::Graphics::UseHelperPrograms(u32 set, bool wait)
{
    SelectVu0Programs(g_Vu0Programs, set, wait);
}
