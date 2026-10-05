#include "platform/graphics.h"

#include "renderer/renderer.h"

#include <ee_regs.h>
#include <gs_privileged.h>
#include <kernel.h>
#include <gif_registers.h>
#include <vif_registers.h>

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
constexpr u32 GifReset = 0x1;           // GIF CTRL.RST

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
    GIF_REG_CTRL = GifReset;
}

// Sony's libgraph's settings, waits and vertical blank callback, and libdma's channels, on PS2SDK's kernel calls and register
// definitions
namespace
{
constexpr u32 VBlankStart = 1 << INTC_VBLANK_S; // I_STAT
constexpr u64 GsInterruptsMasked = 0xFF00;      // IMR
constexpr s16 Interlaced = 1;
constexpr u32 VifPathBusy = 0x3;     // VIF1_STAT.VPS
constexpr u32 GifPathActive = 0xC00; // GIF_STAT.APATH

// The GS's CSR: its events (SIGNAL, FINISH, HSINT, VSINT, EDWINT), FLUSH, RESET, the field shown (NFIELD, FIELD), the FIFO's
// state, the GS's revision and ID
union GsControlStatus
{
    u64 value;
    struct
    {
        u64 signal : 1;
        u64 finish : 1;
        u64 horizontalSync : 1;
        u64 verticalSync : 1;
        u64 drawWindowEnd : 1;
        u64 unused5 : 3;
        u64 flush : 1;
        u64 reset : 1;
        u64 unused10 : 2;
        u64 nextField : 1;
        u64 field : 1;
        u64 fifo : 2;
        u64 revision : 8;
        u64 id : 8;
        u64 unused32 : 32;
    };
};
CHECK_SIZE(GsControlStatus, 8);

// A DMA channel still sending (its CHCR's STR)
bool IsChannelSending(volatile u32* chcr)
{
    DmaChannelControl control;
    control.value = *chcr;
    return control.started != 0;
}

enum ResetGraphMode : s16
{
    ResetGraphFull = 0,
    ResetGraphFlush = 1,
    ResetGraphModeOnly = 5,
};
}

extern "C"
{
    // libgraph's parameters: the display's interlacing, video mode (2 NTSC, 3 PAL) and field mode (1 a frame per field), the GS's
    // revision, and the vertical blank's callback and its interrupt handler
    struct GsParameters
    {
        s16 interlace;
        s16 videoMode;
        s16 fieldMode;
        s16 revision;
        s32 (*vblankCallback)(s32 cause);
        s32 vblankHandler;
    };
    extern GsParameters g_GsParameters RETAIL(D_002E83B0);
    // libdma's channels' registers, by channel
    extern volatile u32* g_DmaChannelRegisters[DmaChannels] RETAIL(DMA_N_CHANNELS);

    GsParameters* sceGsGetGParam() RETAIL(sceGsGetGParam);
    // Mode 0 resets the GS and sets the display's mode up (the interrupts masked, the vertical blank's callback dropped), 1 only
    // flushes the GS, 5 only sets the mode up
    void sceGsResetGraph(s32 mode, s32 interlace, s32 videoMode, s32 fieldMode) RETAIL(sceGsResetGraph);
    // Waits for the next vertical blank: the field it starts when interlaced, else 1
    s32 sceGsSyncV(s32 mode) RETAIL(RenderVSync);
    // Waits until the path to the GS is idle (its DMA channels, VIF1, VU1 and the GIF), whatever the mode
    s32 sceGsSyncPath(s32 mode, u16 timeout) RETAIL(WaitGraphicalDataTransferFinish);
    // The vertical blank's callback set (nullptr: none): returns the one before
    void* sceGsSyncVCallback(s32 (*callback)(s32 cause)) RETAIL(sceGsSyncVCallback);
    volatile u32* sceDmaGetChan(u32 channel) RETAIL(sceDmaGetChan);

    // The waits for the vertical blank start, polling I_STAT, or (while a callback has the interrupt) also the flag the kernel
    // sets with the GS's CSR then, which it returns
    void WaitForVBlank() RETAIL(WaitForVSync);
    u64 WaitForVBlankFlag() RETAIL(WaitForVSyncSet);
}

namespace
{
void AcknowledgeVBlank()
{
    s32 enabled = DI();
    *R_EE_I_STAT = VBlankStart;
    asm volatile("sync.l");
    if (enabled != 0)
    {
        EI();
    }
}

void DropVBlankCallback(GsParameters* parameters)
{
    DisableIntc(INTC_VBLANK_S);
    RemoveIntcHandler(INTC_VBLANK_S, parameters->vblankHandler);
}
}

GsParameters* sceGsGetGParam()
{
    return &g_GsParameters;
}

void sceGsResetGraph(s32 mode, s32 interlace, s32 videoMode, s32 fieldMode)
{
    auto resetMode = static_cast<s16>(mode);
    auto shortInterlace = static_cast<s16>(interlace);
    auto shortVideoMode = static_cast<s16>(videoMode);
    auto shortFieldMode = static_cast<s16>(fieldMode);
    if (resetMode == ResetGraphFlush)
    {
        GsControlStatus flush = {};
        flush.flush = 1;
        *GS_REG_CSR = flush.value;
        return;
    }

    if (resetMode != ResetGraphFull && resetMode != ResetGraphModeOnly)
    {
        return;
    }

    GsParameters* parameters = sceGsGetGParam();
    if (resetMode == ResetGraphFull)
    {
        GsControlStatus reset = {};
        reset.reset = 1;
        *GS_REG_CSR = reset.value;
    }

    parameters->interlace = shortInterlace;
    parameters->videoMode = shortVideoMode;
    GsControlStatus status;
    status.value = *GS_REG_CSR;
    parameters->revision = static_cast<s16>(status.revision);
    if (resetMode == ResetGraphFull)
    {
        GsPutIMR(GsInterruptsMasked);
    }

    parameters->fieldMode = shortFieldMode != 0 ? 1 : 0;
    if (resetMode == ResetGraphFull && parameters->vblankCallback != nullptr)
    {
        DropVBlankCallback(parameters);
        parameters->vblankHandler = 0;
        parameters->vblankCallback = nullptr;
    }

    SetGsCrt(shortInterlace & 1, shortVideoMode & 0xFF, shortFieldMode & 1);
}

void WaitForVBlank()
{
    AcknowledgeVBlank();
    while ((*R_EE_I_STAT & VBlankStart) == 0)
    {
        asm volatile("nop; nop; nop");
    }

    AcknowledgeVBlank();
}

u64 WaitForVBlankFlag()
{
    // What the kernel writes at the vertical blank: retail read the CSR off its stack even when I_STAT ended the wait first
    alignas(8) volatile u32 flag = 0;
    alignas(8) volatile u64 csr = 0;
    SetVSyncFlag(const_cast<u32*>(&flag), const_cast<u64*>(&csr));
    AcknowledgeVBlank();
    while ((*R_EE_I_STAT & VBlankStart) == 0 && flag == 0)
    {
    }

    AcknowledgeVBlank();
    return csr;
}

s32 sceGsSyncV(s32)
{
    GsParameters* parameters = sceGsGetGParam();
    if (parameters->vblankCallback == nullptr)
    {
        WaitForVBlank();
        if (parameters->interlace != Interlaced)
        {
            return 1;
        }

        GsControlStatus status;
        status.value = *GS_REG_CSR;
        return static_cast<s32>(status.field);
    }

    GsControlStatus status;
    status.value = WaitForVBlankFlag();
    if (parameters->interlace != Interlaced)
    {
        return 1;
    }

    return static_cast<s32>(status.field);
}

s32 sceGsSyncPath(s32, u16)
{
    while (IsChannelSending(R_EE_D1_CHCR) || IsChannelSending(R_EE_D2_CHCR) || (*R_EE_VIF1_STAT & VifPathBusy) != 0 ||
           (ReadVpuStat() & Vu1Busy) != 0 || (*R_EE_GIF_STAT & GifPathActive) != 0)
    {
    }

    return 0;
}

void* sceGsSyncVCallback(s32 (*callback)(s32 cause))
{
    GsParameters* parameters = sceGsGetGParam();
    auto* previous = reinterpret_cast<void*>(parameters->vblankCallback);
    if (callback == nullptr)
    {
        DropVBlankCallback(parameters);
        parameters->vblankCallback = nullptr;
        parameters->vblankHandler = 0;
        return previous;
    }

    if (previous != nullptr)
    {
        DropVBlankCallback(parameters);
    }

    parameters->vblankCallback = callback;
    parameters->vblankHandler =
        AddIntcHandler2(INTC_VBLANK_S, reinterpret_cast<s32 (*)(s32, void*, void*)>(callback), -1, nullptr);
    EnableIntc(INTC_VBLANK_S);
    return previous;
}

volatile u32* sceDmaGetChan(u32 channel)
{
    if (channel >= DmaChannels)
    {
        return nullptr;
    }

    return g_DmaChannelRegisters[channel];
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
    // The GS's video mode the renderer set the display up in (sceGsResetGraph's: 2 NTSC, 3 PAL) and its field mode (0)
    extern s32 g_VideoOutMode RETAIL(G_VideoOutMode);
    extern s32 g_VideoFieldMode RETAIL(G_VideoFFMode);
}

namespace
{
constexpr s32 VideoNtsc = 2;
constexpr s32 VideoPal = 3;
}

bool Platform::Graphics::IsPalDisplay()
{
    return g_VideoOutMode == VideoPal;
}

extern "C"
{
    // The display's settings the renderer worked out at start-up (FUN_0019b570): the GS's PCRTC registers' fields
    struct DisplaySettings
    {
        // DISPFB: the frame buffer's base (in 2048 word pages), width (in 64 pixels) and pixel format; then words nothing reads
        // (0, the depth buffer's page, 0 and 0)
        u32 frameBuffer;
        u32 frameWidth;
        u32 pixelFormat;
        u32 unused0C;
        u32 unused10;
        u32 unused14;
        u32 unused18;
        // PMODE; then words nothing reads (0 and 2)
        u32 alpha;
        u32 enable1;
        u32 enable2;
        u32 alphaFromRegister;
        u32 blendWithBackground;
        u32 crtMode;
        u32 alphaOutput;
        u32 unused38;
        u32 unused3C;
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
// The PCRTC registers' fields the settings are ORed into unmasked, by their first bits: PMODE's second circuit on (EN2), CRT mode
// (CRTMD), alpha from ALP (MMOD), alpha output (AMOD), blending with the background (SLBG) and the alpha (ALP); DISPLAY's y (DY),
// magnifications (MAGH, MAGV), width and height (DW, DH); DISPFB's width (FBW) and pixel format (PSM)
constexpr u32 PmodeEnable2Shift = 1;
constexpr u32 PmodeCrtModeShift = 2;
constexpr u32 PmodeAlphaFromRegisterShift = 5;
constexpr u32 PmodeAlphaOutputShift = 6;
constexpr u32 PmodeBlendWithBackgroundShift = 7;
constexpr u32 PmodeAlphaShift = 8;
constexpr u32 DisplayYShift = 12;
constexpr u32 DisplayMagnifyXShift = 23;
constexpr u32 DisplayMagnifyYShift = 27;
constexpr u32 DisplayWidthShift = 32;
constexpr u32 DisplayHeightShift = 44;
constexpr u32 DispfbWidthShift = 9;
constexpr u32 DispfbFormatShift = 15;
// SMODE2: interlaced (INT), a field at a time (FFMD 0)
constexpr u64 InterlacedFields = 1;
// The display's corner on the TV (in its units across and in lines): the second circuit a line lower
constexpr s32 TvLeft = 0x27C;
constexpr s32 TvTop = 0x32;

// The first frame keeps the display the reset set up (the retail G_IsPCRTC_Ready starts at 1)
bool g_SkipDisplaySetUp = true;

// The GS's PCRTC: the circuits' settings from the renderer's (SetupPCRTC)
void SetUpDisplay()
{
    const DisplaySettings& display = g_DisplaySettings;
    u64 mode = display.enable1 | static_cast<u64>(display.enable2) << PmodeEnable2Shift |
               static_cast<u64>(display.crtMode) << PmodeCrtModeShift |
               static_cast<u64>(display.alphaFromRegister) << PmodeAlphaFromRegisterShift |
               static_cast<u64>(display.alphaOutput) << PmodeAlphaOutputShift |
               static_cast<u64>(display.blendWithBackground) << PmodeBlendWithBackgroundShift |
               static_cast<u64>(display.alpha) << PmodeAlphaShift;
    *GS_REG_PMODE = mode;
    *GS_REG_SMODE2 = InterlacedFields;
    u64 area = static_cast<u64>(display.magnifyX) << DisplayMagnifyXShift |
               static_cast<u64>(display.magnifyY) << DisplayMagnifyYShift | static_cast<u64>(display.width) << DisplayWidthShift |
               static_cast<u64>(display.height) << DisplayHeightShift;
    u64 x = static_cast<u32>(display.x + TvLeft);
    *GS_REG_DISPLAY1 = x | static_cast<u64>(static_cast<u32>(display.y + TvTop)) << DisplayYShift | area;
    *GS_REG_DISPLAY2 = x | static_cast<u64>(static_cast<u32>(display.y + TvTop + 1)) << DisplayYShift | area;
    u64 frame = display.frameBuffer | static_cast<u64>(display.frameWidth) << DispfbWidthShift |
                static_cast<u64>(display.pixelFormat) << DispfbFormatShift;
    *GS_REG_DISPFB1 = frame;
    *GS_REG_DISPFB2 = frame;
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
    *R_EE_D2_TADR = reinterpret_cast<u32>(chain) & PhysicalMask;
    // From memory in chain mode
    DmaChannelControl start = {};
    start.fromMemory = 1;
    start.mode = DmaChainMode;
    start.started = 1;
    *R_EE_D2_CHCR = start.value;
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
    WaitForVif1Dma();
    FinishDMATransferAll();
    sceGsSyncPath(0, 0);
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
    WaitForVif1Dma();
    FinishDMATransferAll();
    sceGsSyncPath(0, 0);
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
        *R_EE_D_PCR = WaitOnChannels(bit);
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

    sceGsSyncPath(0, 0);
}

// The VU0 microcode sets and the one loaded (G_UnkDmaRelated), switched by a DMA to VIF0 (renderer/vu0programs.cpp)
void Platform::Graphics::UseHelperPrograms(u32 set, bool wait)
{
    SelectVu0Programs(g_Vu0Programs, set, wait);
}

// The renderer's start (InitRenderer_'s, FUN_0019b570's) and its display's place, on the GS's PCRTC registers' fields above
extern "C"
{
    // The object the retail renderer kept its display in (nothing in it): its functions take it and don't read it
    struct GsDisplay;
    extern GsDisplay g_GsDisplay RETAIL(D_0030A818);
    // The display set up for frames of a size in a video mode (2 NTSC, 3 PAL): the path to the GS reset, the GS reset into the
    // mode, the display circuits' settings worked out from the size (the frame buffer 16 bits a pixel at page 0, magnified to the
    // TV's width), the frame chain's head written, the pages of the buffers the frame is drawn in
    void InitDisplay(GsDisplay* display, s32 width, s32 height, s32 videoMode) RETAIL(FUN_0019b570);
    // The display's offset from the TV's corner (in its units across and in lines)
    void SetDisplayPosition(GsDisplay* display, s32 x, s32 y) RETAIL(FUN_001a0848);

    // The GS's offsets of the screen's middle across and down (2048), the frame's width and height (512 and the renderer's)
    // and whether the game clock was stopped at the last step, none of which anything reads
    extern s32 g_UnreadScreenMiddleX RETAIL(D_0030AAEC);
    extern s32 g_UnreadScreenMiddleY RETAIL(D_0030AAF4);
    extern s16 g_UnreadFrameWidth RETAIL(D_0030AB12);
    extern s16 g_UnreadFrameHeight RETAIL(D_0030AB14);
    extern u8 g_AnimationsStopped RETAIL(D_0030AB16);
}

namespace
{
constexpr s32 FrameWidth = 0x200;
// The display's settings: 16 bits a pixel (PSMCT16), PMODE's alpha 0x80 and its two circuits on, the alpha from ALP
constexpr u32 DisplayPixelFormat = 2;
constexpr u32 DisplayAlpha = 0x80;
constexpr u32 UnusedDisplayWord = 2;
// How far the display moves at the screen offset's ends
constexpr f32 DisplayMoveAcross = 256.0f;
constexpr f32 DisplayMoveDown = 32.0f;

// The display's units across a pixel less one, by the frame's width (others keep what's there)
u32 MagnifyX(u32 width, u32 current)
{
    switch (width)
    {
    case 0x100:
        return 9;
    case 0x140:
        return 7;
    case 0x180:
        return 6;
    case 0x200:
        return 4;
    case 0x280:
        return 3;
    default:
        return current;
    }
}
}

void InitDisplay(GsDisplay*, s32 width, s32 height, s32 videoMode)
{
    g_UnreadScreenMiddleX = GsScreenMiddle;
    g_VideoOutMode = videoMode == VideoPal ? VideoPal : VideoNtsc;
    g_UnreadScreenMiddleY = GsScreenMiddle;
    g_DisplayWidth = width;
    g_DisplayHeight = height;
    g_VideoFieldMode = 0;
    Platform::Graphics::ResetPath();
    sceGsResetGraph(ResetGraphFull, Interlaced, static_cast<s16>(g_VideoOutMode), static_cast<s16>(g_VideoFieldMode));

    DisplaySettings& display = g_DisplaySettings;
    u32 pixels = g_DisplayWidth * g_DisplayHeight;
    display.frameWidth = g_DisplayWidth >> GsWidthShift;
    display.unused3C = UnusedDisplayWord;
    display.alpha = DisplayAlpha;
    display.alphaFromRegister = 1;
    display.frameBuffer = 0;
    display.pixelFormat = DisplayPixelFormat;
    display.unused0C = 0;
    display.unused10 = (g_DrawPixelBytes + g_DisplayPixelBytes) * pixels >> GsPageShift;
    display.unused14 = 0;
    display.unused18 = 0;
    display.unused38 = 0;
    display.enable1 = 1;
    display.enable2 = 1;
    display.blendWithBackground = 0;
    display.crtMode = 0;
    display.alphaOutput = 0;
    display.magnifyX = MagnifyX(g_DisplayWidth, display.magnifyX);
    display.y = 0;
    display.height = g_DisplayHeight - 1;
    display.magnifyY = 0;
    display.x = 0;
    display.width = (display.magnifyX + 1) * g_DisplayWidth;
    WriteFrameHead();
    pixels = g_DisplayWidth * g_DisplayHeight;
    g_FrameBufferPage = pixels * g_DisplayPixelBytes >> GsPageShift;
    g_DepthBufferPage = pixels * (g_DisplayPixelBytes + g_DrawPixelBytes) >> GsPageShift;
}

void SetDisplayPosition(GsDisplay*, s32 x, s32 y)
{
    g_DisplaySettings.y = y;
    g_DisplaySettings.x = x;
}

void Platform::Graphics::StartRenderer(s32 height, bool pal)
{
    auto* memory = static_cast<u8*>(MemoryAllocate2(RendererDmaMemorySize));
    g_UnreadFrameWidth = FrameWidth;
    g_RendererDmaNext = memory;
    g_RendererDmaMemory = memory;
    g_UnreadFrameHeight = static_cast<s16>(height);
    InitDisplay(&g_GsDisplay, FrameWidth, static_cast<s16>(height), pal ? VideoPal : VideoNtsc);
}

void Platform::Graphics::FinishRendererStart()
{
    InitVuPrograms();
    g_RendererDmaNext = CarveDmaMemory(g_RendererDmaNext);
    InitialiseFrameBuckets(&g_FrameBuckets);
    InitialiseSmallBucket(&g_SmallBucket);
    InitialiseLargeBucket(&g_LargeBucket);
    MakeTextureSlots(g_TextureUploadContext);
    ClearRenderedMaterials();
    g_RendererDmaNext = InitialiseInstanceBlocks(g_RendererDmaNext);
    MakeDefaultMaterials();
    InitAlphaPresets();
    InitialiseScreenEffects();
    MakeSharedGifPacket();
}

void Platform::Graphics::MoveDisplay(const Vector2* offset)
{
    s32 x = static_cast<s32>(offset->x * DisplayMoveAcross);
    SetDisplayPosition(&g_GsDisplay, x, static_cast<s32>(offset->y * DisplayMoveDown));
}

void Platform::Graphics::StepAnimations(const TimeClock* clock)
{
    g_AnimationsStopped = clock->flags.running ^ 1;
    AnimateMaterials(clock);
    UpdateParticleWaves(clock);
}
