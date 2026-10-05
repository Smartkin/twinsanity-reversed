#include "renderer.h"

#include "platform/graphics.h"

// VU0's microcode sets (the game's maths helpers, which the asm's macro mode calls with vcallms): the set loaded and the DMA
// chain to VIF0 that loads each (sets 1 to 3; nothing gives set 0 one), a set loaded, and the copier of the third set that puts
// quadwords into VU0's memory

namespace
{
struct Vu0ProgramSets
{
    u32 loaded;
    const void* chains[4];
};

// The pause between two looks at the busy channel
constexpr s32 BusyPause = 0x1D;
}

extern "C"
{
    // The DMA chains to VIF0 of the three sets (in .vutext): the standard one, the culling's and the decals'
    extern const u8 g_StandardVu0Programs[] RETAIL(D_002E6110);
    extern const u8 g_CullingVu0Programs[] RETAIL(D_002E4EB0);
    extern const u8 g_DecalVu0Programs[] RETAIL(D_002E5BD0);
    // GCC 2.9x's initialisation function (for every priority: the sets' chains, set 1 loaded) and the module's global constructor
    void InitVu0Programs(s32 initialise, s32 priority) RETAIL(FUN_002b2070);
    void ConstructVu0ProgramsModule() RETAIL(FUN_002b2230);
}

void SelectVu0Programs(u8* programs, u32 set, bool wait)
{
    auto* sets = reinterpret_cast<Vu0ProgramSets*>(programs);
    if (sets->loaded == set)
    {
        return;
    }

    StartDmaChain(Vif0Channel, sets->chains[set], true);
    sets->loaded = set;
    if (!wait)
    {
        return;
    }

    while (IsDmaChannelBusy(Vif0Channel))
    {
        for (s32 pause = BusyPause; pause >= 0; pause--)
        {
            asm volatile("nop\n\tnop\n\tnop\n\tnop");
        }
    }
}

// The copier (VU0's microprogram 0 of the third set) started with vi01 set, then given eight quadwords at a time in vf01-vf08
// with the address in vi02 and how many blocks are left in vi03, clearing vi01 when it takes them (the interlocked move waits for
// it)
void SendToVu0(u8*, const void* source, s32 quadwords, s32 address)
{
    s32 blocks = quadwords >> 3;
    if ((quadwords & 7) != 0)
    {
        blocks++;
    }

    asm volatile(".set push\n\t"
                 ".set noreorder\n\t"
                 "vnop\n\t"
                 "addi $8, $0, 0x1\n\t"
                 "ctc2.ni $8, $vi1\n\t"
                 "vcallms 0x0\n\t"
                 "addi $9, %2, 0x0\n\t"
                 "nop\n\t"
                 "addi $10, %1, 0x0\n\t"
                 "nop\n\t"
                 "addi $8, %0, 0x0\n\t"
                 "nop\n\t"
                 "ctc2.ni $10, $vi2\n\t"
                 "nop\n\t"
                 "nop\n\t"
                 "nop\n"
                 "1:\n\t"
                 "addi $9, $9, -0x1\n\t"
                 "lqc2 $vf1, 0x0($8)\n\t"
                 "lqc2 $vf2, 0x10($8)\n\t"
                 "lqc2 $vf3, 0x20($8)\n\t"
                 "lqc2 $vf4, 0x30($8)\n\t"
                 "lqc2 $vf5, 0x40($8)\n\t"
                 "lqc2 $vf6, 0x50($8)\n\t"
                 "lqc2 $vf7, 0x60($8)\n\t"
                 "lqc2 $vf8, 0x70($8)\n\t"
                 "ctc2.ni $9, $vi3\n\t"
                 "nop\n\t"
                 "ctc2.i $0, $vi1\n\t"
                 "nop\n\t"
                 "nop\n\t"
                 "nop\n\t"
                 "nop\n\t"
                 "nop\n\t"
                 "addi $8, $8, 0x80\n\t"
                 "nop\n\t"
                 "bgtz $9, 1b\n\t"
                 "nop\n\t"
                 "nop\n\t"
                 ".set pop"
                 :
                 : "r"(source), "r"(address), "r"(blocks)
                 : "$8", "$9", "$10", "memory");
}

void InitVu0Programs(s32 initialise, s32 priority)
{
    if (priority != static_cast<s32>(DefaultInitPriority) || initialise == 0)
    {
        return;
    }

    auto* sets = reinterpret_cast<Vu0ProgramSets*>(g_Vu0Programs);
    sets->loaded = 0;
    sets->chains[Platform::Graphics::CullingPrograms] = g_CullingVu0Programs;
    sets->chains[Platform::Graphics::StandardPrograms] = g_StandardVu0Programs;
    sets->chains[Platform::Graphics::DecalPrograms] = g_DecalVu0Programs;
    SelectVu0Programs(g_Vu0Programs, Platform::Graphics::StandardPrograms, false);
}

void ConstructVu0ProgramsModule()
{
    InitVu0Programs(1, DefaultInitPriority);
}
