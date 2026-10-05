#include "renderer.h"

#include "platform/graphics.h"

#include <libgs.h>

namespace
{
// An MPG loads at most 256 instructions (of 8 bytes, two a quadword): the programs go in 240 at a time
constexpr u32 MpgChunk = 0xF0;
constexpr u32 InstructionBytes = 8;
constexpr u32 InstructionsPerQuadwordShift = 1;
// The programs loaded when a material needs them, from 14 on (0-13 are the first bucket's every frame)
constexpr u32 FirstLoadedProgram = 0xE;
// The two GIF tag templates the programs start their packets from: triangle fans (PRIM preset) ending the GIF's packet, their
// loops 0 (the programs put theirs in), the first's vertexes writing ST, RGBAQ and XYZ2, the other's RGBAQ and XYZ2
constexpr u64 TexturedVertexRegisters = GifDescriptors(GifSt, GifRgbaq, GifXyz2);
constexpr u64 ColouredVertexRegisters = GifDescriptors(GifRgbaq, GifXyz2);

u64 TriangleFanTemplate(u32 registerCount)
{
    GifTag tag = {};
    tag.endOfPacket = 1;
    tag.setsPrim = 1;
    tag.prim = GS_PRIM_TRI_FAN;
    tag.registerCount = registerCount;
    return tag.value;
}

// A CNT tag of nothing but VIF1's FLUSHE and STCYCL, then the program's code by REF tags, an MPG of 240 instructions each, to
// address. Returns where it ends
u32* WriteProgram(u32* at, const VuProgram& program, u32 address)
{
    at[0] = CountTag;
    at[1] = 0;
    at[2] = VifFlushE;
    at[3] = VifCycle1;
    at += 4;
    u32 left = program.size;
    const u8* source = reinterpret_cast<const u8*>(program.code);
    do
    {
        u32 count = left <= MpgChunk ? left : MpgChunk;
        at[0] = count >> InstructionsPerQuadwordShift | ReferenceTag;
        at[1] = Address(source);
        at[2] = 0;
        at[3] = address | count << VifCountShift | VifMpg;
        at += 4;
        left -= count;
        source += count * InstructionBytes;
        address += count;
    } while (left != 0);

    return at;
}
}

extern "C"
{
    void PutVUProgramsIntoDMAPipeline()
    {
        // The order the first bucket gets them in
        constexpr u32 Programs[] = {1, 2, 10, 13, 3, 11, 5, 6, 7, 8};
        for (u32 program : Programs)
        {
            PutProgramIntoDMAPipeline(program, 0);
        }
    }

    void PutProgramIntoDMAPipeline(u32 program, u32 bucketIndex)
    {
        RenderBucket& bucket = g_FrameBuckets.buckets[bucketIndex];
        const VuProgram& code = g_VuPrograms[program];
        u32* at = WriteProgram(BeginPacket(bucket), code, code.address);
        EndPacket(bucket, reinterpret_cast<u8*>(at));
    }

    void ForgetLoadedPrograms()
    {
        g_VuProgramTime = 0;
        for (u32 program = FirstLoadedProgram; program < VuProgramCount; program++)
        {
            ForgetLoadedProgram(program);
        }
    }

    void ForgetLoadedProgram(u32 program)
    {
        g_VuPrograms[program].loaded = ProgramNotLoaded;
    }

    u8* LoadProgramForMaterial(u8* packet, u32 program)
    {
        VuProgram& entry = g_VuPrograms[program];
        if (entry.loaded == g_VuProgramTime)
        {
            return packet;
        }

        u32 address = g_VuProgramNext;
        entry.address = address;
        entry.loaded = g_VuProgramTime;
        entry.bucketAddresses[g_VuProgramBucket] = static_cast<u16>(address);
        packet = WriteProgramUpload(packet, program);
        g_VuProgramNext += entry.size;
        return packet;
    }

    u8* WriteProgramUpload(u8* packet, u32 program)
    {
        const VuProgram& entry = g_VuPrograms[program];
        return reinterpret_cast<u8*>(WriteProgram(reinterpret_cast<u32*>(packet), entry, entry.bucketAddresses[g_VuProgramBucket]));
    }

    // The first bucket's packet of what the VU1 programs share: VIF1's double buffers, the entries of programs 9 to 13 the
    // programs go on to, and the GIF tags they start their packets from, each at its place in VU1's memory
    void QueueSharedProgramData()
    {
        RenderBucket& bucket = g_FrameBuckets.buckets[0];
        u32* at = BeginPacket(bucket);
        at[0] = CountTag | 8;
        at[1] = 0;
        at[2] = VifFlushA;
        at[3] = 0;
        at[4] = VifCycle1;
        at[5] = g_VuBufferBase | VifBase;
        at[6] = (g_VuBufferOffset - g_VuBufferBase) | VifOffset;
        at[7] = g_VuRegisterAddress | VifUnpackV4;
        at[8] = g_VuRegisterValue1;
        at[9] = g_VuRegisterValue2;
        at[10] = g_VuRegisterValue1;
        at[11] = g_VuRegisterValue2;
        at[12] = g_VuEntriesAddress1 | VifUnpackV3;
        at[13] = g_VuPrograms[9].address;
        at[14] = g_VuPrograms[10].address;
        at[15] = g_VuPrograms[11].address;
        // The element's third word isn't written: VU1 gets whatever the buffer had there
        at[16] = g_VuEntriesAddress2 | VifUnpackV3;
        at[17] = g_VuPrograms[12].address;
        at[18] = g_VuPrograms[13].address;
        at[20] = 0;
        at[21] = 0;
        at[22] = 0;
        at[23] = g_VuGifTemplateAddress1 | VifUnpackV4;
        auto* templates = reinterpret_cast<u64*>(at + 24);
        templates[0] = TriangleFanTemplate(3);
        templates[1] = TexturedVertexRegisters;
        at[28] = 0;
        at[29] = 0;
        at[30] = 0;
        at[31] = g_VuGifTemplateAddress2 | VifUnpackV4;
        templates = reinterpret_cast<u64*>(at + 32);
        templates[0] = TriangleFanTemplate(2);
        templates[1] = ColouredVertexRegisters;
        EndPacket(bucket, reinterpret_cast<u8*>(at + 36));
    }
}

void Platform::Graphics::FinishFrame()
{
    ForgetLoadedPrograms();
}

// The registration of the programs at start-up: the five the renderer's own packets use and five more are resident (loaded with
// the first bucket every frame, one after another from the start of VU1's micro memory), the shader types' are loaded when a
// material needs them, after the resident ones. Each type's registration also keeps its program's code and size
struct ProgramCode
{
    const u64* code;
    s32 size;
};

extern "C"
{
    // The renderer's own programs (1, 2, 0xA, 0xD and 0xB) and their sizes in instructions
    extern const u64 g_RendererProgram1Code[] RETAIL(D_002DB0E0);
    extern s16 g_RendererProgram1Size RETAIL(G_MicroCode_1_InstructionAmt);
    extern const u64 g_RendererProgram2Code[] RETAIL(D_002DA060);
    extern s16 g_RendererProgram2Size RETAIL(G_MicroCode_2_InstructionAmt);
    extern const u64 g_RendererProgram3Code[] RETAIL(D_002E4430);
    extern s16 g_RendererProgram3Size RETAIL(G_MicroCode_3_InstructionAmt);
    extern const u64 g_RendererProgram4Code[] RETAIL(D_002E47C0);
    extern s16 g_RendererProgram4Size RETAIL(G_MicroCode_4_InstructionAmt);
    extern const u64 g_RendererProgram5Code[] RETAIL(D_002E2C60);
    extern s16 g_RendererProgram5Size RETAIL(G_MicroCode_5_InstructionAmt);

    // Each registered program: its index's variable, its code in .vutext, its size in .data (instructions) and the copy of both
    extern const u64 g_ParameterCode[] RETAIL(D_002E28D0);
    extern s16 g_ParameterSize RETAIL(D_002EC3B0);
    extern ProgramCode g_ParameterCopy RETAIL(D_0030ABBC);
    extern const u64 g_BlendParameterCode[] RETAIL(D_002E21F0);
    extern s16 g_BlendParameterSize RETAIL(D_002EC3A0);
    extern ProgramCode g_BlendParameterCopy RETAIL(D_0030ABCC);
    extern const u64 g_BlendNoShapesCode[] RETAIL(D_002E2450);
    extern s16 g_BlendNoShapesSize RETAIL(D_002EC3A4);
    extern ProgramCode g_BlendNoShapesCopy RETAIL(D_0030ABD4);
    extern const u64 g_BlendFirstShapeCode[] RETAIL(D_002E25B0);
    extern s16 g_BlendFirstShapeSize RETAIL(D_002EC3A8);
    extern ProgramCode g_BlendFirstShapeCopy RETAIL(D_0030ABDC);
    extern const u64 g_BlendNextShapeCode[] RETAIL(D_002E2750);
    extern s16 g_BlendNextShapeSize RETAIL(D_002EC3AC);
    extern ProgramCode g_BlendNextShapeCopy RETAIL(D_0030ABE4);
    extern u32 g_ShaderType04Program RETAIL(D_00309E30);
    extern const u64 g_ShaderType04Code[] RETAIL(D_002E3B40);
    extern s16 g_ShaderType04Size RETAIL(D_002EC3C4);
    extern ProgramCode g_ShaderType04Copy RETAIL(D_0030ABC4);
    extern u32 g_ShaderType01Program RETAIL(D_00309DFC);
    extern const u64 g_ShaderType01Code[] RETAIL(D_002DF700);
    extern s16 g_ShaderType01Size RETAIL(D_002EC388);
    extern ProgramCode g_ShaderType01Copy RETAIL(D_0030ABAC);
    extern u32 g_ShaderType03Program RETAIL(D_00309DC0);
    extern const u64 g_ShaderType03Code[] RETAIL(D_002DB3E0);
    extern s16 g_ShaderType03Size RETAIL(D_002EC360);
    extern ProgramCode g_ShaderType03Copy RETAIL(D_00309DB8);
    extern u32 g_ShaderType02Program RETAIL(D_00309DC8);
    extern const u64 g_ShaderType02Code[] RETAIL(D_002DBEB0);
    extern s16 g_ShaderType02Size RETAIL(D_002EC368);
    extern ProgramCode g_ShaderType02Copy RETAIL(D_0030ABB4);
    extern u32 g_ShaderType0AProgram RETAIL(D_00309E34);
    extern const u64 g_ShaderType0ACode[] RETAIL(D_002E3EC0);
    extern s16 g_ShaderType0ASize RETAIL(D_002EC3C8);
    extern ProgramCode g_ShaderType0ACopy RETAIL(D_0030ABEC);
    extern u32 g_ShaderType0BProgram RETAIL(D_00309E0C);
    extern const u64 g_ShaderType0BCode[] RETAIL(D_002E1A90);
    extern s16 g_ShaderType0BSize RETAIL(D_002EC39C);
    extern ProgramCode g_ShaderType0BCopy RETAIL(D_0030ABF4);
    extern u32 g_ShaderType0CProgram RETAIL(D_00309DAC);
    extern const u64 g_ShaderType0CCode[] RETAIL(D_002DA080);
    extern s16 g_ShaderType0CSize RETAIL(D_002EC350);
    extern ProgramCode g_ShaderType0CCopy RETAIL(D_0030ABFC);
    extern u32 g_ShaderType0FProgram RETAIL(D_00309E24);
    extern const u64 g_ShaderType0FCode[] RETAIL(D_002E2DE0);
    extern s16 g_ShaderType0FSize RETAIL(D_002EC3B8);
    extern ProgramCode g_ShaderType0FCopy RETAIL(D_0030AC04);
    extern u32 g_ShaderType10Program RETAIL(D_00309E04);
    extern const u64 g_ShaderType10Code[] RETAIL(D_002E1000);
    extern s16 g_ShaderType10Size RETAIL(D_002EC398);
    extern ProgramCode g_ShaderType10Copy RETAIL(D_0030AC0C);
    extern u32 g_ShaderType11Program RETAIL(D_00309E2C);
    extern const u64 g_ShaderType11Code[] RETAIL(D_002E3720);
    extern s16 g_ShaderType11Size RETAIL(D_002EC3C0);
    extern ProgramCode g_ShaderType11Copy RETAIL(D_0030AC14);
    extern u32 g_ShaderType15Program RETAIL(D_00309DCC);
    extern const u64 g_ShaderType15Code[] RETAIL(D_002DC5D0);
    extern s16 g_ShaderType15Size RETAIL(D_002EC36C);
    extern ProgramCode g_ShaderType15Copy RETAIL(D_0030AC2C);
    extern u32 g_ShaderType16Program RETAIL(D_00309DEC);
    extern const u64 g_ShaderType16Code[] RETAIL(D_002DE590);
    extern s16 g_ShaderType16Size RETAIL(D_002EC380);
    extern ProgramCode g_ShaderType16Copy RETAIL(D_0030AC34);
    extern u32 g_ShaderType17Program RETAIL(D_00309DE4);
    extern const u64 g_ShaderType17Code[] RETAIL(D_002DD780);
    extern s16 g_ShaderType17Size RETAIL(D_002EC378);
    extern ProgramCode g_ShaderType17Copy RETAIL(D_0030AC3C);
    extern u32 g_ShaderType19Program RETAIL(D_00309DE8);
    extern const u64 g_ShaderType19Code[] RETAIL(D_002DDF40);
    extern s16 g_ShaderType19Size RETAIL(D_002EC37C);
    extern ProgramCode g_ShaderType19Copy RETAIL(D_0030AC4C);
    extern u32 g_ShaderType1AProgram RETAIL(D_00309DE0);
    extern const u64 g_ShaderType1ACode[] RETAIL(D_002DCFB0);
    extern s16 g_ShaderType1ASize RETAIL(D_002EC374);
    extern ProgramCode g_ShaderType1ACopy RETAIL(D_0030AC54);
    extern u32 g_ShaderType1BProgram RETAIL(D_00309DF4);
    extern ProgramCode g_ShaderType1BCopy RETAIL(D_0030AC5C);
    extern u32 g_ShaderType1CProgram RETAIL(D_00309DB4);
    extern const u64 g_ShaderType1CCode[] RETAIL(D_002DB170);
    extern s16 g_ShaderType1CSize RETAIL(D_002EC35C);
    extern ProgramCode g_ShaderType1CCopy RETAIL(D_0030AC64);
    extern u32 g_ShaderType1EProgram RETAIL(D_00309DF0);
    extern const u64 g_ShaderType1ECode[] RETAIL(D_002DF230);
    extern s16 g_ShaderType1ESize RETAIL(D_002EC384);
    extern ProgramCode g_ShaderType1ECopy RETAIL(D_0030AC6C);
    extern u32 g_ShaderType1FProgram RETAIL(D_00309DC4);
    extern const u64 g_ShaderType1FCode[] RETAIL(D_002DB690);
    extern s16 g_ShaderType1FSize RETAIL(D_002EC364);
    extern ProgramCode g_ShaderType1FCopy RETAIL(D_0030AC74);
    extern u32 g_ShaderType20Program RETAIL(D_00309E28);
    extern const u64 g_ShaderType20Code[] RETAIL(D_002E3400);
    extern s16 g_ShaderType20Size RETAIL(D_002EC3BC);
    extern ProgramCode g_ShaderType20Copy RETAIL(D_0030AC7C);
    extern u32 g_ShaderType0DProgram RETAIL(D_00309DB0);
    extern const u64 g_ShaderType0DCode[] RETAIL(D_002DAE70);
    extern s16 g_ShaderType0DSize RETAIL(D_002EC354);
    extern ProgramCode g_ShaderType0DCopy RETAIL(D_0030AC84);
    extern u32 g_ShaderType12Program RETAIL(D_00309DDC);
    extern const u64 g_ShaderType12Code[] RETAIL(D_002DCBD0);
    extern s16 g_ShaderType12Size RETAIL(D_002EC370);
    extern ProgramCode g_ShaderType12Copy RETAIL(D_0030AC1C);
    extern u32 g_ShaderType13Program RETAIL(D_00309DA8);
    extern const u64 g_ShaderType13Code[] RETAIL(D_002D9D90);
    extern s16 g_ShaderType13Size RETAIL(D_002EC348);
    extern ProgramCode g_ShaderType13Copy RETAIL(D_0030AC24);
    extern u32 g_ShaderType18Program RETAIL(D_00309E08);
    extern const u64 g_ShaderType18Code[] RETAIL(D_002E0960);
    extern s16 g_ShaderType18Size RETAIL(D_002EC394);
    extern ProgramCode g_ShaderType18Copy RETAIL(D_0030AC44);
}

namespace
{
constexpr u32 VuMicroMemory = 0x800;
constexpr u32 RendererPrograms[] = {1, 2, 0xA, 0xD, 0xB};

// The program's entry: its code, its size made even, where it goes (after the resident programs), and not loaded. A resident one
// takes its place there
void Register(u32 index, const u64* code, s16 size, bool resident)
{
    VuProgram& program = g_VuPrograms[index];
    program.code = code;
    program.size = static_cast<u32>(size + 1) & ~1u;
    program.address = g_VuProgramStart;
    program.resident = resident ? 1 : 0;
    program.loaded = ProgramNotLoaded;
    if (resident)
    {
        g_VuProgramStart += program.size;
    }
}
}

extern "C"
{
    void RegisterVuProgram(const u64* code, s32 size, u32 index) RETAIL(RegisterVU_Program);
    // Type 0's: none
    void RegisterShaderType00Program() RETAIL(FUN_001db738);
    void RegisterParameterProgram() RETAIL(FUN_001dcea0);
    void RegisterBlendParameterProgram() RETAIL(FUN_001dc9a8);
    void RegisterBlendNoShapesProgram() RETAIL(FUN_001dcb78);
    void RegisterBlendFirstShapeProgram() RETAIL(FUN_001dcc90);
    void RegisterBlendNextShapeProgram() RETAIL(FUN_001dcda8);
    void RegisterShaderType04Program() RETAIL(FUN_001dd350);
    void RegisterShaderType01Program() RETAIL(FUN_001dc350);
    void RegisterShaderType03Program() RETAIL(FUN_001db350);
    void RegisterShaderType02Program() RETAIL(FUN_001db568);
    void RegisterShaderType0AProgram() RETAIL(FUN_001dd438);
    void RegisterShaderType0BProgram() RETAIL(FUN_001dc7f8);
    void RegisterShaderType0CProgram() RETAIL(FUN_001dae68);
    void RegisterShaderType0FProgram() RETAIL(FUN_001dd070);
    void RegisterShaderType10Program() RETAIL(FUN_001dc568);
    void RegisterShaderType11Program() RETAIL(FUN_001dd1f0);
    void RegisterShaderType15Program() RETAIL(FUN_001db650);
    void RegisterShaderType16Program() RETAIL(FUN_001dbf68);
    void RegisterShaderType17Program() RETAIL(FUN_001dbc30);
    void RegisterShaderType19Program() RETAIL(FUN_001dbe80);
    void RegisterShaderType1AProgram() RETAIL(FUN_001db980);
    void RegisterShaderType1BProgram() RETAIL(FUN_001dc138);
    void RegisterShaderType1CProgram() RETAIL(FUN_001daf98);
    void RegisterShaderType1EProgram() RETAIL(FUN_001dc050);
    void RegisterShaderType1FProgram() RETAIL(FUN_001db480);
    void RegisterShaderType20Program() RETAIL(FUN_001dd108);
    void RegisterShaderType0DProgram() RETAIL(FUN_001daf00);
    void RegisterShaderType12Program() RETAIL(FUN_001db7b8);
    void RegisterShaderType13Program() RETAIL(FUN_001dac88);
    void RegisterShaderType18Program() RETAIL(FUN_001dc6c8);

    void RegisterVuProgram(const u64* code, s32 size, u32 index)
    {
        Register(index, code, static_cast<s16>(size), true);
    }

    void RegisterShaderType00Program()
    {
    }

    void RegisterParameterProgram()
    {
        Register(g_ParameterProgram, g_ParameterCode, g_ParameterSize, true);
        g_ParameterCopy = {g_ParameterCode, g_ParameterSize};
    }

    void RegisterBlendParameterProgram()
    {
        Register(g_BlendParameterProgram, g_BlendParameterCode, g_BlendParameterSize, true);
        g_BlendParameterCopy = {g_BlendParameterCode, g_BlendParameterSize};
    }

    void RegisterBlendNoShapesProgram()
    {
        Register(g_BlendNoShapesProgram, g_BlendNoShapesCode, g_BlendNoShapesSize, true);
        g_BlendNoShapesCopy = {g_BlendNoShapesCode, g_BlendNoShapesSize};
    }

    void RegisterBlendFirstShapeProgram()
    {
        Register(g_BlendFirstShapeProgram, g_BlendFirstShapeCode, g_BlendFirstShapeSize, true);
        g_BlendFirstShapeCopy = {g_BlendFirstShapeCode, g_BlendFirstShapeSize};
    }

    void RegisterBlendNextShapeProgram()
    {
        Register(g_BlendNextShapeProgram, g_BlendNextShapeCode, g_BlendNextShapeSize, true);
        g_BlendNextShapeCopy = {g_BlendNextShapeCode, g_BlendNextShapeSize};
    }

    void RegisterShaderType04Program()
    {
        Register(g_ShaderType04Program, g_ShaderType04Code, g_ShaderType04Size, false);
        g_ShaderType04Copy = {g_ShaderType04Code, g_ShaderType04Size};
    }

    void RegisterShaderType01Program()
    {
        Register(g_ShaderType01Program, g_ShaderType01Code, g_ShaderType01Size, false);
        g_ShaderType01Copy = {g_ShaderType01Code, g_ShaderType01Size};
    }

    void RegisterShaderType03Program()
    {
        Register(g_ShaderType03Program, g_ShaderType03Code, g_ShaderType03Size, false);
        g_ShaderType03Copy = {g_ShaderType03Code, g_ShaderType03Size};
    }

    void RegisterShaderType02Program()
    {
        Register(g_ShaderType02Program, g_ShaderType02Code, g_ShaderType02Size, false);
        g_ShaderType02Copy = {g_ShaderType02Code, g_ShaderType02Size};
    }

    void RegisterShaderType0AProgram()
    {
        Register(g_ShaderType0AProgram, g_ShaderType0ACode, g_ShaderType0ASize, false);
        g_ShaderType0ACopy = {g_ShaderType0ACode, g_ShaderType0ASize};
    }

    void RegisterShaderType0BProgram()
    {
        Register(g_ShaderType0BProgram, g_ShaderType0BCode, g_ShaderType0BSize, false);
        g_ShaderType0BCopy = {g_ShaderType0BCode, g_ShaderType0BSize};
    }

    void RegisterShaderType0CProgram()
    {
        Register(g_ShaderType0CProgram, g_ShaderType0CCode, g_ShaderType0CSize, false);
        g_ShaderType0CCopy = {g_ShaderType0CCode, g_ShaderType0CSize};
    }

    void RegisterShaderType0FProgram()
    {
        Register(g_ShaderType0FProgram, g_ShaderType0FCode, g_ShaderType0FSize, false);
        g_ShaderType0FCopy = {g_ShaderType0FCode, g_ShaderType0FSize};
    }

    void RegisterShaderType10Program()
    {
        Register(g_ShaderType10Program, g_ShaderType10Code, g_ShaderType10Size, false);
        g_ShaderType10Copy = {g_ShaderType10Code, g_ShaderType10Size};
    }

    void RegisterShaderType11Program()
    {
        Register(g_ShaderType11Program, g_ShaderType11Code, g_ShaderType11Size, false);
        g_ShaderType11Copy = {g_ShaderType11Code, g_ShaderType11Size};
    }

    void RegisterShaderType15Program()
    {
        Register(g_ShaderType15Program, g_ShaderType15Code, g_ShaderType15Size, false);
        g_ShaderType15Copy = {g_ShaderType15Code, g_ShaderType15Size};
    }

    void RegisterShaderType16Program()
    {
        Register(g_ShaderType16Program, g_ShaderType16Code, g_ShaderType16Size, false);
        g_ShaderType16Copy = {g_ShaderType16Code, g_ShaderType16Size};
    }

    void RegisterShaderType17Program()
    {
        Register(g_ShaderType17Program, g_ShaderType17Code, g_ShaderType17Size, false);
        g_ShaderType17Copy = {g_ShaderType17Code, g_ShaderType17Size};
    }

    void RegisterShaderType19Program()
    {
        Register(g_ShaderType19Program, g_ShaderType19Code, g_ShaderType19Size, false);
        g_ShaderType19Copy = {g_ShaderType19Code, g_ShaderType19Size};
    }

    void RegisterShaderType1AProgram()
    {
        Register(g_ShaderType1AProgram, g_ShaderType1ACode, g_ShaderType1ASize, false);
        g_ShaderType1ACopy = {g_ShaderType1ACode, g_ShaderType1ASize};
    }

    void RegisterShaderType1BProgram()
    {
        Register(g_ShaderType1BProgram, g_ShaderType01Code, g_ShaderType01Size, false);
        g_ShaderType1BCopy = {g_ShaderType01Code, g_ShaderType01Size};
    }

    void RegisterShaderType1CProgram()
    {
        Register(g_ShaderType1CProgram, g_ShaderType1CCode, g_ShaderType1CSize, false);
        g_ShaderType1CCopy = {g_ShaderType1CCode, g_ShaderType1CSize};
    }

    void RegisterShaderType1EProgram()
    {
        Register(g_ShaderType1EProgram, g_ShaderType1ECode, g_ShaderType1ESize, false);
        g_ShaderType1ECopy = {g_ShaderType1ECode, g_ShaderType1ESize};
    }

    void RegisterShaderType1FProgram()
    {
        Register(g_ShaderType1FProgram, g_ShaderType1FCode, g_ShaderType1FSize, false);
        g_ShaderType1FCopy = {g_ShaderType1FCode, g_ShaderType1FSize};
    }

    void RegisterShaderType20Program()
    {
        Register(g_ShaderType20Program, g_ShaderType20Code, g_ShaderType20Size, false);
        g_ShaderType20Copy = {g_ShaderType20Code, g_ShaderType20Size};
    }

    void RegisterShaderType0DProgram()
    {
        Register(g_ShaderType0DProgram, g_ShaderType0DCode, g_ShaderType0DSize, false);
        g_ShaderType0DCopy = {g_ShaderType0DCode, g_ShaderType0DSize};
    }

    void RegisterShaderType12Program()
    {
        Register(g_ShaderType12Program, g_ShaderType12Code, g_ShaderType12Size, false);
        g_ShaderType12Copy = {g_ShaderType12Code, g_ShaderType12Size};
    }

    void RegisterShaderType13Program()
    {
        Register(g_ShaderType13Program, g_ShaderType13Code, g_ShaderType13Size, false);
        g_ShaderType13Copy = {g_ShaderType13Code, g_ShaderType13Size};
    }

    void RegisterShaderType18Program()
    {
        Register(g_ShaderType18Program, g_ShaderType18Code, g_ShaderType18Size, false);
        g_ShaderType18Copy = {g_ShaderType18Code, g_ShaderType18Size};
    }

    // Every program unregistered, the renderer's and the types' registered, and the micro memory past the resident programs split
    // in two for the buckets to load programs into
    void InitVuPrograms()
    {
        g_VuProgramStart = 0;
        for (u32 index = 1; index < VuProgramCount; index++)
        {
            VuProgram& program = g_VuPrograms[index];
            program.size = 0;
            program.address = 0;
            program.loaded = ProgramNotLoaded;
            program.resident = 0;
        }

        RegisterVuProgram(g_RendererProgram1Code, g_RendererProgram1Size, RendererPrograms[0]);
        RegisterVuProgram(g_RendererProgram2Code, g_RendererProgram2Size, RendererPrograms[1]);
        RegisterVuProgram(g_RendererProgram3Code, g_RendererProgram3Size, RendererPrograms[2]);
        RegisterVuProgram(g_RendererProgram4Code, g_RendererProgram4Size, RendererPrograms[3]);
        RegisterVuProgram(g_RendererProgram5Code, g_RendererProgram5Size, RendererPrograms[4]);
        RegisterShaderType00Program();
        RegisterParameterProgram();
        RegisterBlendParameterProgram();
        RegisterBlendNoShapesProgram();
        RegisterBlendFirstShapeProgram();
        RegisterBlendNextShapeProgram();
        RegisterShaderType04Program();
        RegisterShaderType01Program();
        RegisterShaderType03Program();
        RegisterShaderType02Program();
        RegisterShaderType0AProgram();
        RegisterShaderType0BProgram();
        RegisterShaderType0CProgram();
        RegisterShaderType0FProgram();
        RegisterShaderType10Program();
        RegisterShaderType11Program();
        RegisterShaderType15Program();
        RegisterShaderType16Program();
        RegisterShaderType17Program();
        RegisterShaderType19Program();
        RegisterShaderType1AProgram();
        RegisterShaderType1BProgram();
        RegisterShaderType1CProgram();
        RegisterShaderType1EProgram();
        RegisterShaderType1FProgram();
        RegisterShaderType20Program();
        RegisterShaderType0DProgram();
        RegisterShaderType12Program();
        RegisterShaderType13Program();
        RegisterShaderType18Program();
        u32 start = g_VuProgramStart;
        g_VuProgramNext = start;
        g_VuProgramRegion2 = start;
        g_VuProgramTime = 0;
        g_VuProgramRegion1 = start + ((VuMicroMemory - start) >> 1);
    }
}

// The module's static initialisation (GCC 2.9x's pair of functions): the programs' table's constructors do nothing, and a word
// nothing reads is set
extern "C"
{
    extern s32 g_VuProgramsUnread RETAIL(D_0030A890);
    void ConstructVuProgramsModule(u32 initialize, u32 priority) RETAIL(FUN_001d9018);
    void InitVuProgramsModule() RETAIL(FUN_001dd4d0);

    void ConstructVuProgramsModule(u32 initialize, u32 priority)
    {
        if (priority == DefaultInitPriority && initialize != 0)
        {
            g_VuProgramsUnread = -1;
        }
    }

    void InitVuProgramsModule()
    {
        ConstructVuProgramsModule(1, DefaultInitPriority);
    }
}
