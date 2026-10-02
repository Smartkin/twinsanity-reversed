#pragma once

#include "common.h"
#include "gcc2.h"
#include "game/chunkdata.h"
#include "game/controllers.h"
#include "game/decals.h"
#include "game/math.h"
#include "game/renderer.h"
#include "game/graphicstables.h"
#include "game/view.h"

// The PS2 renderer's DMA: the channels it sends on, the chains of DMA packets the render buckets are written into, and the
// buckets. A chain has two buffers a frame apart (one is sent while the other is filled) and two small ones movies use, whose
// buffers' memory is the movie decoder's then. A bucket is a writer's place in its chain: each packet ends with a "next" tag the
// one after fills the address of, and linking the frame's buckets fills each one's last with the next one's first. The retail asm
// still writes most of the packets

// DMA tags (their ID in the first word: CNT sends the quadwords after it, NEXT goes on at its address, REF sends the ones at its
// address, CALL goes there until a RET, END stops) and VIF1's codes (in a tag's second half, or in what a CNT sends): STCYCL of
// 1 and 1, OFFSET, BASE, FLUSHE, FLUSH, FLUSHA, MSCAL, MSCNT, MPG, DIRECT, and UNPACK of one S-32, V3-32 or V4-32 element (VifUnpackV4Count
// and VifUnpackV4Bytes, of V4-8 elements, take the count in bits 16-23)
constexpr u32 CountTag = 0x10000000;
constexpr u32 NextTag = 0x20000000;
constexpr u32 ReferenceTag = 0x30000000;
constexpr u32 CallTag = 0x50000000;
constexpr u32 ReturnTag = 0x60000000;
constexpr u32 EndTag = 0x70000000;
constexpr u32 VifCycle1 = 0x01000101;
constexpr u32 VifOffset = 0x02000000;
constexpr u32 VifBase = 0x03000000;
constexpr u32 VifFlushE = 0x10000000;
constexpr u32 VifFlush = 0x11000000;
constexpr u32 VifFlushA = 0x13000000;
constexpr u32 VifMscal = 0x14000000;
constexpr u32 VifMscnt = 0x17000000;
constexpr u32 VifMpg = 0x4A000000;
constexpr u32 VifDirect = 0x50000000;
constexpr u32 VifUnpackS = 0x60010000;
constexpr u32 VifUnpackV3 = 0x68010000;
constexpr u32 VifUnpackV4 = 0x6C010000;
constexpr u32 VifUnpackV4Count = 0x6C000000;
constexpr u32 VifUnpackV4Bytes = 0x6E000000;

inline u32 Address(const void* pointer)
{
    return reinterpret_cast<u32>(pointer);
}

// The renderer's DMA channels 0-9 (SetDmaRegisterPointers, not the SIF's 5-7): their registers, their bit of D_STAT and whether
// a chain is being sent
struct RendererDmaChannel
{
    volatile u32* registers;
    u32 statusBit;
    u32 sending;
};

// A chain of DMA packets
struct DmaChain
{
    u8* buffers[2];
    u8* movieBuffers[2];
    // Where the next packet goes, where the buffer being filled starts, its size in quadwords and which buffer it is
    u8* next;
    u8* start;
    u32 capacity;
    u32 buffer;
    // 1 for the frame's buckets' chains, 2 and 8 for the two buckets of their own
    u32 kind;
    u32 unknown24;
};
CHECK_SIZE(DmaChain, 0x28);

// A writer's place in its chain: its first tag and the last one, which the next packet's address goes into. Its materials take
// turns with VU1's two buffers and load their programs into one of two regions of VU1's micro memory (kept when the bucket starts
// over), and the key of the programs its last material loaded saves loading them again
struct RenderBucket
{
    u32* first;
    // A writer started with two tags takes packets in before its last: here
    u32* insertion;
    u32* last;
    u8 vuBuffer;
    u8 unknown0D;
    u8 programRegion;
    u8 unknown0F;
    s32 chain;
    u32 unknown14;
    u64 lastKey;
    u32 unknown20;
    // The material the bucket's last 2D drawing was set up for
    struct Material* last2DMaterial;
    u32 unknown28;
    // The packet the bucket's last material called first: the next one with the same skips its CALL
    u32 lastCall;
    // The joints' matrices the bucket's last skin sent VU1: the next skin with the same doesn't send them again
    u32 lastJoints;
    u32 unknown34;
};
CHECK_SIZE(RenderBucket, 0x38);

// Buckets after 8 bytes of something else: the frame's 28 (drawn in their order) and two of their own
template <u32 Count>
struct RenderBucketSet
{
    u32 unknown00[2];
    RenderBucket buckets[Count];
};
using FrameBuckets = RenderBucketSet<28>;
using SingleBucket = RenderBucketSet<1>;
CHECK_SIZE(FrameBuckets, 0x628);

struct TextureSlot;

// A texture of the game's (an RM2's texture item) as the GS sees it: its size (TEX0's powers of two), how many mip levels it has
// (1 none), its pixel format (the upload's buffer is 64 pixels wide unless it's 32 bit), the format it's uploaded in, TEX0's
// colour component and function, where it and its mips go in its slot (in 64 words) and their buffer widths (in 64 pixels),
// where its palette goes, its transfer chain (a handle of the disk manager's: the GS's transfer registers and the image), the
// slot of GS memory it's in and whether its registers are the second context's
struct Texture
{
    u16 unknown00;
    u16 unknown02;
    u16 widthPower;
    u16 heightPower;
    u8 levels;
    u8 pixelFormat;
    u8 uploadFormat;
    u8 hasAlpha;
    u8 unknown0C;
    u8 function;
    u8 unknown0E[2];
    u32 levelOffsets[7];
    u32 levelWidths[7];
    u32 paletteOffset;
    u8 unknown4C[0x54 - 0x4C];
    s32 transfer;
    TextureSlot* slot;
    u16 unknown5C;
    u16 secondContext;
};
CHECK_OFFSET(Texture, paletteOffset, 0x48);
CHECK_OFFSET(Texture, secondContext, 0x5E);

// A slot of GS memory textures are uploaded to: its address (in 64 words), the buckets the texture was sent in since it came,
// when it was last used (a count of uses) and the texture
struct TextureSlot
{
    u32 gsAddress;
    u32 buckets;
    u32 used;
    Texture* owner;
};
CHECK_SIZE(TextureSlot, 0x10);

struct ShaderAnimation;

// The bits of a shader's settings (their first bit): alpha blending (PRMODE's ABE), the alpha preset (4 bits), the alpha test
// (TEST's ATE, ATST, AREF and AFAIL), the destination alpha test (DATE, DATM), the depth test (ZTST, 2 bits), Gouraud shading
// (IIP), texturing (TME, with TEX1 and CLAMP sent), STQ texture coordinates (FST cleared), fog (FGE), the GS's second context, the
// U and V scrolls' modes (3 bits each, ShaderScroll), an alpha formula of its own in place of the preset (A, B, C and D of 2
// bits, then FIX), linear filtering (MMAG, and MMIN without mips), no FBA, anti-aliasing (AA1, only type 0 sends it), no depth
// writes (ZMSK), the colour from the animation and an animation read with the shader. Bits 23-25 and 57 are read from the files
// and never used
enum ShaderSetting : u32
{
    SettingBlends = 0,
    SettingPreset = 1,
    SettingAlphaTest = 5,
    SettingAlphaMethod = 6,
    SettingAlphaReference = 9,
    SettingAlphaFail = 17,
    SettingDestinationTest = 19,
    SettingDestinationMode = 20,
    SettingDepthTest = 21,
    SettingGouraud = 26,
    SettingTextured = 27,
    SettingStq = 28,
    SettingFog = 29,
    SettingSecondContext = 30,
    SettingUScroll = 32,
    SettingVScroll = 35,
    SettingOwnAlpha = 38,
    SettingAlphaFormula = 39,
    SettingAlphaFix = 47,
    SettingLinear = 55,
    SettingNoFba = 56,
    SettingAntiAliased = 58,
    SettingNoDepthWrites = 59,
    SettingAnimatedColour = 60,
    SettingAnimation = 61,
};

// A scroll's modes: none, the animation's offset, a phase going round from 0 to 1, and the sine and cosine of the phase's turn
enum ShaderScroll : u32
{
    ScrollNone = 0,
    ScrollAnimated = 1,
    ScrollWrapped = 2,
    ScrollSine = 3,
    ScrollCosine = 4,
};

// A material's shader (a type of the game's, its vtable 0x6C bytes in): its colour (bytes), its texture (a texture item, the GS
// uploads' view of it 0xC bytes in), its UV scroll, and its GS registers (A+D pairs, the packet's GIF tag first) and how many
// quadwords they are
struct Shader
{
    // Its settings' bits (what it draws with and how, ShaderSetting)
    u64 settings;
    u8 unknown08[4];
    // A font material's first shader: the page of the font it draws
    u8 fontPage;
    u8 colour[4];
    u8 unknown11[3];
    u8* texture;
    // TEX1's mip level choice: K and L
    s16 lodK;
    s16 lodL;
    u8 unknown1C[4];
    // Bytes the tools left (TT Lab's LeftoverVector), only compared when equal shaders are looked for
    f32 leftover[4];
    f32 shaderColour[4];
    // The U and V scrolls' phases and speeds (a turn a second, ShaderScroll)
    f32 scrollPhases[2];
    f32 scrollSpeeds[2];
    f32 scroll[2];
    u32 registers;
    u32 registerCount;
    // Its texture's ID (0 none) and its type
    u32 textureId;
    u32 type;
    // Its animation (UV scroll and colour tracks), if it has one
    ShaderAnimation* animation;
    const GccVTableEntry* vtable;
};
CHECK_OFFSET(Shader, lodL, 0x1A);
CHECK_OFFSET(Shader, leftover, 0x20);
CHECK_OFFSET(Shader, type, 0x64);
CHECK_OFFSET(Shader, animation, 0x68);
CHECK_OFFSET(Shader, registers, 0x58);
CHECK_SIZE(Shader, 0x70);

// The cloth shaders (types 0x17 and 0x1A): 16 rows of three wave phases (type 0x1A's mode 2 has a second set), the waves VU1
// gets (16 quadwords made every frame), the mode, the speed and the amplitude (type 0x1A's one per axis)
struct ClothShader : Shader
{
    Vector4 phases[16];
    Vector4 secondPhases[16];
    u32 waves;
    u32 mode;
    f32 speed;
    f32 amplitudes[3];
};
CHECK_OFFSET(ClothShader, phases, 0x70);
CHECK_OFFSET(ClothShader, waves, 0x270);
CHECK_OFFSET(ClothShader, amplitudes, 0x27C);

extern "C"
{
    // The shaders' base constructor and a shader's texture set by its ID (none for 0)
    Shader* ShaderConstruct(Shader* shader) RETAIL(FUN_001cd070);
    void SetShaderTexture(Shader* shader, u32 id) RETAIL(FUN_001daaa8);
    // Type 0xE's, 0x12's, 0x13's, 0x18's and 0x1C's set-ups (their vtables' function 13: the type number), and types 0x12's and
    // 0x13's vtables (the particle pages' shaders)
    void ShaderType0ESetUp(Shader* shader) RETAIL(FUN_001dc2b8);
    void ShaderType12SetUp(Shader* shader) RETAIL(FUN_001d9eb8);
    void ShaderType13SetUp(Shader* shader) RETAIL(FUN_001d9f70);
    extern const GccVTableEntry g_ShaderType12VTable[] RETAIL(PrecompiledShader__Type_0x12_Methods);
    extern const GccVTableEntry g_ShaderType13VTable[] RETAIL(PrecompiledShader__Type_0x13_Methods);
    void ShaderType18SetUp(struct ScreenCopyShader* shader) RETAIL(FUN_001da1f8);
    void ShaderType1CSetUp(struct WaveShader* shader) RETAIL(FUN_001da3f8);
    // Type 0x1C's frame update: its waves moved on by some seconds
    u32 ShaderType1CUpdate(struct WaveShader* shader, f32 seconds) RETAIL_N32(FUN_001cf450);
    // The colour bytes from the float colour: the red only (what reaches the VU1 programs)
    void SetShaderColourByte(Shader* shader) RETAIL(SetShaderColorByte);
}

// The screen copies (types 0x10, 0x11 and 0x18): a float VU1 gets with the screen's corner
struct ScreenCopyShader : Shader
{
    f32 cornerValue;
};

// Type 0x1C's shader: 16 rows of three wave phases, its waves (16 quadwords made every frame), their speed and what VU1 gets
// after the UV scroll (where the cloth shaders send their amplitude)
struct WaveShader : Shader
{
    Vector4 phases[16];
    u32 waves;
    f32 speed;
    f32 amplitude;
};
CHECK_OFFSET(WaveShader, waves, 0x170);
CHECK_OFFSET(WaveShader, amplitude, 0x178);

// A material: its shaders, the key of the VU1 programs they draw with (a bit per shader type), the render bucket it's drawn in,
// the writer of the frame's draws of it (in its bucket's chain, put into the bucket at the frame's end), the packet its drawing
// calls first (none, the font renderer's, the shared GIF tag's) and whether it's in the frame's list of materials drawn
struct Material
{
    Shader* shaders[4];
    u32 shaderCount;
    u32 unknown14;
    u64 activatedShaders;
    u32 bucket;
    u32 unknown24;
    RenderBucket writer;
    u32 call;
    u8 listed;
    u8 unknown65;
    // Skins drew it straight into its bucket this frame: its writer isn't put there
    u8 drawnDirectly;
};
CHECK_OFFSET(Material, call, 0x60);

struct InstanceBlock;

// A model's place for the block of its instances a frame draws: the block while it's open, whether its materials need the eye
// and the model's matrix (environment maps), and whether it's a billboard (a shader of its turns it to face the camera)
struct InstanceBlockOwner
{
    InstanceBlock* block;
    u8 needsEye;
    u8 billboard;
};

// A block of a frame's instances of one model, which the model's packets call: its owner (pointing back at it while it's open),
// where it is (a RET tag with VIF1's UNPACK of the instances, then the instances), how many instances it has, their kind (5 a
// rigid model's, 10 FUN_0019c410's), the instances' size and the block's capacity (quadwords)
struct InstanceBlock
{
    InstanceBlockOwner* owner;
    u8* data;
    u16 count;
    u16 kind;
    u16 size;
    u16 capacity;
    u32 unknown10;
};
CHECK_SIZE(InstanceBlock, 0x14);

// The graphics resources the chunks share (the game's tables', game/graphicstables.h), as the PS2 has them. Each starts with the
// tables' header

// A texture: the size of its data in the file (the GS's header, 0x20 bytes of the tools' memory and the image), and the GS's view
// of it (its image in the disk manager)
struct GameTexture
{
    ResourceHeader header;
    u32 size;
    Texture texture;
};
CHECK_SIZE(GameTexture, 0x6C);

// A model (a rigid model's submodels, 0x1C bytes): how many submodels, each one's vertex count, packet size, the size of data
// the files have after the packet (read and dropped) and packet (a handle of the disk manager's)
struct RigidModelData
{
    ResourceHeader header;
    u32 subModelCount;
    u32* vertexCounts;
    u32* sizes;
    u32* extraSizes;
    s32* subModels;
};
CHECK_SIZE(RigidModelData, 0x1C);

// A rigid model as an OGI or the scenery (a mesh) draws it (0x20 bytes): its submodels' materials, the model, the block of its
// instances, how many submodels (materials) it has and the model's ID
struct RigidModel
{
    ResourceHeader header;
    MaterialResource** materials;
    RigidModelData* model;
    InstanceBlockOwner instances;
    u32 count;
    u32 modelId;
};
CHECK_OFFSET(RigidModel, count, 0x18);
CHECK_SIZE(RigidModel, 0x20);

// A skin as an OGI draws it (0x24 bytes): its submodels' materials, vertex counts, packets (handles of the disk manager's), how
// many, their packets' sizes, and the vertexes all of them have (added to what the memory held: its constructor doesn't clear it)
struct Skin
{
    ResourceHeader header;
    MaterialResource** materials;
    u32* vertexCounts;
    s32* subModels;
    u32 count;
    u32* sizes;
    u32 vertexTotal;
    u32 unknown20;
};
CHECK_OFFSET(Skin, count, 0x14);
CHECK_SIZE(Skin, 0x24);

// A part of a blend skin's submodel (0x28 bytes): each shape's offsets of its vertexes (where they are, their size in quadwords and
// how many, bytes VU1 unpacks), the packet of the part's vertexes, how many shapes, three factors VU1 gets with each shape's
// weight (the first part's go with every part's), something read with it and the packet's size
struct BlendPart
{
    u32* shapeAddresses;
    u32* shapeSizes;
    u32* shapeCounts;
    u32 packet;
    u8 shapeCount;
    f32 shapeFactors[3];
    u32 unknown20;
    u32 packetSize;
};
CHECK_OFFSET(BlendPart, shapeFactors, 0x14);
CHECK_SIZE(BlendPart, 0x28);

struct BlendSubModel
{
    BlendPart** parts;
    u32 count;
};

// A blend skin as an OGI draws it (0x18 bytes): its submodels' materials, the submodels, how many, and how many shapes they have
struct BlendSkin
{
    ResourceHeader header;
    MaterialResource** materials;
    BlendSubModel** subModels;
    u32 count;
    u32 shapeCount;
};
CHECK_OFFSET(BlendSkin, count, 0x10);
CHECK_SIZE(BlendSkin, 0x18);

// A level of detail of the scenery's (0x24 bytes): its meshes, how many, and the squares of its distances: the nearest and
// farthest it's drawn at (-1 no limit) and where each mesh gives way to the next
struct Lod
{
    ResourceHeader header;
    RigidModel** meshes;
    u8 count;
    u32 distances[5];
};
CHECK_OFFSET(Lod, distances, 0x10);
CHECK_SIZE(Lod, 0x24);

// An object whose models' instances go into blocks of placed models (dynamic scenery): its matrices to camera space and to the
// screen and the view's clip vector, the view it's drawn in, how (1 unclipped), something whose matrix 0xC0 bytes in takes the
// camera where its world matrix applies, and its vtable (the sixth function hands its world matrix)
struct PlacedObject
{
    u8 unknown00[0x40];
    Matrix4x4 toCamera;
    Matrix4x4 toScreen;
    Vector4 clip;
    u8 unknownD0[0xF4 - 0xD0];
    RenderView* view;
    u32 mode;
    u8 unknownFC[4];
    u8* unknown100;
    const GccVTableEntry* vtable;
};
CHECK_OFFSET(PlacedObject, vtable, 0x104);

// A chunk's sky (0x50 bytes): its meshes, and the matrix of the half-size buffer's screen this frame
struct Sky
{
    ResourceHeader header;
    u32 count;
    RigidModel** models;
    Matrix4x4 toScreen;
};
CHECK_OFFSET(Sky, toScreen, 0x10);
CHECK_SIZE(Sky, 0x50);

// A particle system's block header (in the particle data): a DMA tag sending its next two quadwords to VU1, those (0.5 and the
// scale in the first, the hexagons' distortion in the second), and the packet that sets the system's texture page up
struct ParticleHeader
{
    u32 tag[4];
    f32 values[4];
    f32 distortion[4];
    u8 packet[0x10];
};


// A model of one material whose packet is drawn after the render target's (the screen models FUN_0015c478 draws): the material,
// the packet, the block of its instances
struct ScreenModel
{
    Material* material;
    u32 packet;
    InstanceBlockOwner instances;
};

// A VU1 program: its code, where it goes in VU1's micro memory (where it was put last, for one loaded when needed) and its size
// (in instructions), the material it was loaded for last (a count of materials, -1 when it isn't loaded) and where it went for
// each bucket
struct VuProgram
{
    const u64* code;
    u32 address;
    u32 size;
    u32 loaded;
    // Loaded with the first bucket every frame (the registration's first programs), not when a material needs it
    u8 resident;
    u8 unknown11;
    u16 bucketAddresses[28];
    u16 unknown4A;
};
CHECK_SIZE(VuProgram, 0x4C);

extern "C"
{
    extern RendererDmaChannel g_RendererDma[10] RETAIL(G_DMA_N_CHANNELS);
    // Ten chains: the label's size (0x78) is splat's, the pad after it is the rest. The allocation checks for eleven
    extern DmaChain g_DmaChains[10] RETAIL(G_DmaChains_Array_);
    extern s32 g_DmaChainCount RETAIL(G_TotalDMA_Chains);
    extern u32 g_DmaQuadwordsAllocated RETAIL(G_DMA_ByteStream_TagsAllocatedAmt);
    // The two regions of the chains' buffers, and where the next movie buffers go (from the first region's start: while they're
    // used, the frames' buffers aren't)
    extern u8* g_DmaFirstBuffers RETAIL(G_DMA_ByteStream_Beg_2);
    extern u8* g_DmaSecondBuffers RETAIL(G_UnkDMA_0x187040_ByteStream_2);
    extern u8* g_DmaMovieNext RETAIL(G_UnkDMA_0x187040_ByteStream_1);
    extern u8 g_DmaReady RETAIL(D_00309B48);
    // The buckets fill the chains' movie buffers
    extern u8 g_MovieBuckets RETAIL(D_00309B4C);
    extern FrameBuckets g_FrameBuckets RETAIL(D_003239D8);

    // The two regions of the chains' buffers out of the memory given; returns the memory after them
    u8* CarveDmaMemory(u8* memory) RETAIL(FUN_00181fa0);
    // The frame's buckets' three chains (the first 5 buckets', the next 16', the last 7'), and the buckets started
    void InitialiseFrameBuckets(FrameBuckets* buckets) RETAIL(InitDMA_Manager);
    // A bucket of its own with a chain of 100 quadwords, of 10000
    void InitialiseSmallBucket(SingleBucket* bucket) RETAIL(FUN_0017e510);
    void InitialiseLargeBucket(SingleBucket* bucket) RETAIL(FUN_0017e3c8);
    // A writer emptied
    void RenderBucketConstruct(RenderBucket* bucket) RETAIL(FUN_001a0098);
    // A material without shaders (its writer empty, the packet it calls first 1), and destroyed: its shaders deleted
    Material* MaterialConstruct(Material* material) RETAIL(FUN_001c0838);
    void MaterialDestroy(Material* material, u32 flags) RETAIL(FUN_001c0880);
    // A writer of its own starts in the chain of a frame bucket, with one "next" tag or two
    void StartWriter(RenderBucket* writer, u32 bucket) RETAIL(FUN_00182280);
    void StartWriterTwoTags(RenderBucket* writer, u32 bucket) RETAIL(FUN_00182308);
    // Takes count times size bytes (rounded down to quadwords) and a quadword from the chain of the set's first bucket (every
    // caller hands it the frame's buckets); returns where they start
    u8* AllocDmaTags(FrameBuckets* buckets, u32 count, u32 size);
    // The buckets start over in the chains' other buffers, with the vector units' programs queued again
    void ResetRenderBuckets(FrameBuckets* buckets);
    // The buckets are linked into one chain that VIF1's channel is started on (from the vertical blank's interrupt or not), then
    // start over
    void LinkRenderBuckets(FrameBuckets* buckets, bool fromInterrupt);

    // The VU1 programs (43: the label's size is splat's, the pad after it is the rest), and where the data they share goes in
    // VU1's memory: VIF1's double buffers' base and offset, two registers' values, the entries of programs 9 to 13, the two GIF
    // tag templates
    extern VuProgram g_VuPrograms[43] RETAIL(G_Shader_VU_Programs);
    // The programs loaded when materials need them: a count of the materials that loaded programs (the programs loaded for the
    // current one have it), the bucket of the current material, where the next program goes
    extern u32 g_VuProgramTime RETAIL(D_0030AB70);
    extern u32 g_VuProgramBucket RETAIL(D_0030AB74);
    extern u32 g_VuProgramNext RETAIL(D_0030AB78);
    // The two regions of VU1's micro memory buckets load programs into, and the start of the one the current material loaded its
    // programs in (the shaders' packets go by it)
    extern u32 g_VuProgramRegion1 RETAIL(D_0030AB68);
    extern u32 g_VuProgramRegion2 RETAIL(D_0030AB6C);
    extern u32 g_VuProgramStart RETAIL(G_RegisteredVuInstructionsAmt);
    // VU1's two buffers of a material's data, and where the one it takes next goes
    extern u32 g_VuBuffer1 RETAIL(D_00309CE4);
    extern u32 g_VuBuffer2 RETAIL(D_00309CE8);
    extern u32 g_VuBufferPlace RETAIL(D_00309CB8);
    // The program whose entry (and a place past it) every material hands VU1
    extern u32 g_ParameterProgram RETAIL(G_MicroCode_6_Index);
    extern s16 g_ParameterProgramOffset RETAIL(D_002EC3B2);
    // The packets materials can call: the shared GIF tag's and the render target's
    extern u32 g_SharedGifPacket RETAIL(G_SomeGifTag);
    // What the scene being drawn is drawn into (set with the view)
    extern RenderTargetDescription* g_RenderTarget RETAIL(G_FontRendererRel);
    // What the texture uploads are handed by the materials (unused)
    extern u8 D_0030A820[0x18];
    extern u32 g_VuBufferBase RETAIL(D_00309CEC);
    extern u32 g_VuBufferOffset RETAIL(D_00309CF0);
    extern u32 g_VuRegisterAddress RETAIL(D_00309CB0);
    extern u32 g_VuRegisterValue1 RETAIL(D_00309CF4);
    extern u32 g_VuRegisterValue2 RETAIL(D_00309CF8);
    extern u32 g_VuEntriesAddress1 RETAIL(D_00309CBC);
    extern u32 g_VuEntriesAddress2 RETAIL(D_00309CC0);
    extern u32 g_VuGifTemplateAddress1 RETAIL(D_00309CC4);
    extern u32 g_VuGifTemplateAddress2 RETAIL(D_00309CCC);

    // GS memory's texture slots: the ring of them, how many, the next one tried, the uses counted, the large textures' slot
    extern TextureSlot* g_TextureSlots RETAIL(G_TexRelArray);
    extern u32 g_TextureSlotCount RETAIL(G_TexRelArraySize);
    extern u32 g_TextureSlotNext RETAIL(G_TexRelArrayNextIndex);
    extern u32 g_TextureTime RETAIL(D_0030AB88);
    extern TextureSlot g_LargeTextureSlot RETAIL(G_UnkTexRel);

    // The slots made (the renderer's start)
    void FUN_001bc8f0(void* unused);
    // The texture into GS memory for the bucket: kept when it's in a slot already (sent again when the bucket hasn't had it yet),
    // the large textures' slot (192 pixels wide or more) or the ring's oldest slot otherwise. Writes the upload at packet, returns where it ends (packet
    // when nothing's sent, or no slot was free)
    u8* FUN_001bc9e0(void* context, u8* packet, Texture* texture, u32 bucket);
    // The upload of the texture into the slot at packet; returns where it ends
    u8* SetTextureDma_(void* context, u8* packet, Texture* texture, TextureSlot* slot);
    // Every slot let go of (and the uses counted from 0 but when keepTime), one slot, the large textures', and the first slot
    // (its address returned: a buffer of GS memory to draw into)
    void FUN_001c0f08(void* context, u32 keepTime);
    void FUN_001c0f88(u32 index);
    void FUN_001c0fb8(void* unused);
    u32 FUN_001c0fe0();

    // The VU1 programs queued into the first bucket, a program into a bucket, and the first bucket's packet of the data the
    // programs share
    void PutVUProgramsIntoDMAPipeline();
    void PutProgramIntoDMAPipeline(u32 program, u32 bucket);
    void FUN_001bc650();
    // The programs loaded when needed are forgotten (every frame), one of them, a program loaded for the current material's
    // bucket unless it was for this material (the packet's end returned), and its upload to where it goes for the bucket
    void FUN_001da718();
    void FUN_001da758(u32 program);
    u8* FUN_001da880(u8* packet, u32 program);
    u8* FUN_001da918(u8* packet, u32 program);

    // The material into its bucket's packet: the programs its shaders need loaded (unless the bucket's last material had them),
    // a CALL of what its drawing shares, the entries VU1 goes on to, each shader's texture uploaded and its packet. Returns where
    // the packet ends
    u8* RenderMaterial(Material* material, u8* packet, u32 mode);
    u8* FUN_001a0dd0(u8* packet, u32 call);
    // The material's CALL (anything with its call 0x60 bytes in)
    u8* FUN_001c09e0(const Material* material, u8* packet);
    u8* FUN_001dcf40(u8* packet, u32* counter, u32 mode);
    // The frame's draws of the material into its bucket, after what they need set up (the programs, its shaders' packets, the
    // CALL); every listed material's (but the ones skins drew directly), the list forgotten (the writers too) and emptied
    void FlushMaterial(Material* material) RETAIL(FUN_001bab38);
    u32 FlushMaterials() RETAIL(FUN_001c0c20);
    void ForgetMaterials() RETAIL(FUN_001c0ba0);
    void InitShadersRenderedAmt();
    // TEXFLUSH and the texture's TEX0 (and MIPTBP1 and 2 for its mips) sent to the GIF at packet, for its slot; returns where it
    // ends. The texture of the material's shader uploaded for the bucket, and its registers
    u8* WriteTextureRegisters(void* context, u8* packet, const Texture* texture) RETAIL(FUN_001bcbf8);
    // The same into VU1's memory at the counter (it counts them), with the GIF tag of them and the registers before them at the
    // tag's place; how many registers that is
    u8* WriteTextureRegistersToVu(void* context, u8* packet, const Texture* texture, u32* counter, u32 tagPlace)
        RETAIL(FUN_001bce30);
    u32 TextureRegisterCount(void* context, const Texture* texture) RETAIL(FUN_001c10f0);
    u8* FUN_001c0ab8(const Material* material, u8* packet, u32 shader, u32 bucket);
    u8* FUN_001c0b00(const Material* material, u8* packet, u32 shader);

    // The 2D drawing's packet in progress: its tag, its material, its GIF tag, whether its vertexes have ST coordinates and how
    // many registers it set
    extern u32* g_2DTag RETAIL(D_0030AB20);
    extern Material* g_2DMaterial RETAIL(G_ShaderRel_3);
    extern u64* g_2DGifTag RETAIL(D_0030AB28);
    extern u8 g_2DUsesSt RETAIL(D_0030AB2C);
    extern u32 g_2DPairCount RETAIL(D_0030AB30);
    // A REF of the material's first shader's GS registers, sent to the GIF directly
    u8* WriteShaderRegisters(const Material* material, u8* packet, u32 unused) RETAIL(FUN_001c0a08);
    // 2D drawing with the material in its bucket (set up unless the bucket's last 2D drawing had it): returns where its
    // registers go, which the vertexes' functions write and count; its end finishes the packet's tags at end (the GIF tag of a
    // triangle strip)
    u8* Begin2D(Material* material) RETAIL(FUN_001a67f8);
    void End2D(u8* end) RETAIL(FUN_001a6920);
    // A vertex's colour (RGBA bytes), position (XYZ2, XYZ3 that doesn't draw) and texture coordinates
    u8* Write2DColour(u8* packet, u32 colour) RETAIL(FUN_001ab548);
    u8* Write2DVertex(u8* packet, f32 x, f32 y, u32 z, u32 withoutKick) RETAIL(FUN_001ab5b8);
    u8* Write2DTexCoord(u8* packet, f32 s, f32 t) RETAIL(FUN_001ab628);
    // The material listed among the frame's and its writer started, with one tag or two
    void StartMaterialWriter(Material* material) RETAIL(FUN_001c0920);
    void StartMaterialWriterTwoTags(Material* material) RETAIL(FUN_001c0980);

    // What the skin being drawn is drawn with (the OGI's drawer sets them): its matrix, its inverse (what takes the camera into
    // its space), its lights' directions (a row each), their colours and the ambient light; the same of the blend skin
    extern const Matrix4x4* g_SkinMatrix RETAIL(G_InstTransformMat);
    extern const Matrix4x4* g_SkinInverse RETAIL(D_0030AB94);
    extern const Matrix4x4* g_SkinLights RETAIL(D_0030AB9C);
    extern const Vector4* g_SkinLightColours RETAIL(D_0030ABA0);
    extern const Vector4* g_SkinAmbient RETAIL(D_0030ABA4);
    extern const Matrix4x4* g_BlendSkinMatrix RETAIL(D_00309D68);
    extern const Matrix4x4* g_BlendSkinInverse RETAIL(D_00309D6C);
    extern const Matrix4x4* g_BlendSkinLights RETAIL(D_00309D70);
    extern const Vector4* g_BlendSkinLightColours RETAIL(D_00309D74);
    extern const Vector4* g_BlendSkinAmbient RETAIL(D_00309D78);
    // VU1's places of a skin's data: the joints' matrices, the skin's data in its buffer, and in that the clipped's (a skin's
    // eye's matrix too), the eye (before the data), a blend skin's inverse
    extern u32 g_VuJoints RETAIL(G_CONST_D);
    extern u32 g_VuSkinData RETAIL(D_00309D2C);
    extern u32 g_VuSkinClip RETAIL(D_00309D1C);
    extern u32 g_VuSkinEye RETAIL(D_00309D08);
    extern u32 g_VuBlendInverse RETAIL(D_00309D24);
    // The blend shapes' two buffers (the shapes take turns) and where VU1 learns them
    extern u32 g_VuBlendBuffer1 RETAIL(G_BLEND_SKIN_ANIM_DATA_VU_ADDR_BEGIN);
    extern u32 g_VuBlendBuffer2 RETAIL(G_BLEND_SKIN_ANIM_DATA_VU_ADDR_END);
    extern u32 g_VuBlendBuffersPlace RETAIL(D_00309CC8);
    // The program whose entry (and a place past it) the blend skins' materials hand VU1
    extern u32 g_BlendParameterProgram RETAIL(G_MicroCode_7_Index);
    extern s16 g_BlendParameterProgramOffset RETAIL(D_002EC3A2);

    // A node's matrix made again from its rotation and position when they changed
    void RotateAndTranslate(void* node);
    // The skin drawn straight into its materials' buckets: the joints' matrices (unless the bucket has them), its data, its
    // material's set-up and each submodel's packet. Mode 2 draws it clipped
    void SetSkinDMA(Skin* skin, const Matrix4x4* joints, u32 jointCount, u32 mode);
    // A skin's data for shaders that need the eye: its matrix to camera space and the eye in its space
    u8* FUN_001be720(const Skin* skin, u8* packet, const Matrix4x4* toCamera, u32 buffer);
    // The blend skin drawn like a skin (always in VU1's second buffer), its submodels' shapes blended by their weights
    void SetBlendSkinDMA(BlendSkin* skin, const Matrix4x4* joints, u32 jointCount, const f32* weights, const s32* shapes,
                         const s32* shapeCount, u32 mode);
    // A blend skin's material set up: RenderMaterial with the blend program's entries, without a turn of VU1's buffers
    u8* FUN_001bbc08(Material* material, u8* packet, u32* counter, u32 mode);
    u8* FUN_001dca48(u8* packet, u32* counter, u32 mode);
    // The submodel's parts: each one's vertexes, with the shapes given blended in
    u8* FUN_001c1ac0(const BlendSubModel* subModel, u8* packet, const f32* weights, const s32* shapes, const s32* shapeCount);
    u8* CreateSubBlendDMA_Chain_(const BlendPart* part, u8* packet, const f32* weights, const s32* shapes,
                                 const s32* shapeCount, const f32* factors);
    // The renderer's DMA channels' registers (still asm)
    void SetDmaRegisterPointers();
    // The screen effects the game's flags ask for (still asm)
    void FUN_001b9a68();

    // The materials drawn this frame (the label's size is splat's, D_003D6074 is the rest), how many
    extern Material* g_RenderedMaterials[500] RETAIL(G_MaterialShaderArray_);
    extern u32 g_RenderedMaterialCount RETAIL(G_ShadersRendered_);

    // The instance blocks: 800 a frame, in two regions of memory a frame each (the frame's, and where the next block goes)
    extern InstanceBlock* g_InstanceBlocks RETAIL(G_0x320_ArrayOf_0x14_SizeStruct);
    extern u32 g_InstanceBlockCount RETAIL(D_00309BFC);
    extern u8* g_InstanceFirstRegion RETAIL(D_00309BE8);
    extern u8* g_InstanceSecondRegion RETAIL(D_00309BEC);
    extern u8* g_InstanceRegion RETAIL(D_00309BF0);
    extern u8* g_InstanceBlockNext RETAIL(D_00309BF4);
    // Where VU1 gets the instances, and where it's told that
    extern u32 g_VuInstances RETAIL(D_00309CDC);
    extern u32 g_VuInstancesPlace RETAIL(D_00309CB4);

    // The instance blocks' regions out of the memory given and their list made; returns the memory after them
    u8* InitialiseInstanceBlocks(u8* memory) RETAIL(FUN_001a0e38);
    // The open blocks closed (the frame's end), the other region taken
    u32 CloseInstanceBlocks() RETAIL(FUN_0019cdc8);
    // The rigid model's instance (its matrix, the lights) into its block, a new one when it has none open: returns the new
    // block for the model's packets to call, nullptr when it went into the open one (or there was no room)
    u8* SetRigidModelRenderDMA(InstanceBlockOwner* owner, const Matrix4x4* matrix, u32 mode, const Vector4* lightDirections,
                               const Vector4* lightColours, const Vector4* ambient);
    // The rigid model drawn: its instance, and when that started a block, each submodel's packet with the block into its
    // material's writer. Mode 1 draws it unclipped
    void SetRigidModelDma_(RigidModel* model, const Matrix4x4* matrix, const Vector4* lightDirections,
                           const Vector4* lightColours, const Vector4* ambient, u32 mode);
    // A placed model's instance into its block (a new one when it has none open), its matrices the caller's: returns the new
    // block, nullptr when it went into the open one or there was no room. A billboard (the owner's unknown05) turns to face the
    // camera first; the eye comes from the object's world matrix (the one given without an object)
    u8* SetPlacedModelRenderDMA(InstanceBlockOwner* owner, const Matrix4x4* toScreen, u32 mode, const Matrix4x4* toCamera,
                                const Vector4* clip, const Matrix4x4* world, PlacedObject* object) RETAIL(FUN_0019c410);
    // The placed object's model drawn like a rigid model; a default mesh's (clipped, the material set up straight into its
    // bucket and the submodels put in before its writer's last tag)
    void SetPlacedModelDMA(RigidModel* model, PlacedObject* object, const Matrix4x4* world) RETAIL(FUN_001bef80);
    void SetDefaultMeshDMA(RigidModel* model, const Matrix4x4* toScreen, const Matrix4x4* toCamera, const Matrix4x4* world)
        RETAIL(FUN_001bf188);
    // The material's set-up straight into its bucket, its writer's draws after it (the first shader's packet and program)
    void StartMaterialDirectly(Material* material) RETAIL(FUN_001bb8e0);
    // A screen model drawn: its instance into its block, and when that started one, the material's writer (started with two
    // tags and put into its bucket at once) takes the CALLs of the render target's packet, the block and the model's packet
    void SetScreenModelDMA(ScreenModel* model, const Matrix4x4* toScreen, u32 mode, const Matrix4x4* toCamera,
                           const Vector4* clip) RETAIL(FUN_001a6a28);

    // A block of particles drawn with the material (its writer's packet): the system's header with its scale (and a hexagon's
    // distortion) sent before them
    void DrawParticleBlock(u8* block, ParticleHeader* header, const Matrix4x4* matrix, Material* material, f32 scale)
        RETAIL_N32(DrawParticleBlock);
    void DrawHexagonParticleBlock(u8* block, ParticleHeader* header, const Matrix4x4* matrix, Material* material, f32 scale,
                                  f32 distortionX, f32 distortionY) RETAIL_N32(DrawHexagonParticleBlock);
    // The frame's decals of each type with the type's material, the lists emptied
    void DrawDecals(DecalData* decals);

    // The view's matrix to the screen made the half-size buffer's (the old one kept), and put back
    void UseHalfSizeScreen(RenderView* view, const RenderTargetDescription* target) RETAIL(FUN_0027b6c0);
    void RestoreViewScreenMatrix(RenderView* view) RETAIL(FUN_0027b730);
    // The shadows' half-size buffer made in bucket 21 (the frame's alpha and depth shrunk into it) and the drawing set up for
    // the shadows into it; the scenery is drawn into it next
    void StartShadows(void* shadows) RETAIL(FUN_001c97d8);
    // The shadows drawn into the half-size buffer taken away from the screen, a quarter of it (bucket 21), the half-size buffer
    // cleared first
    void ApplyShadows(void* shadows) RETAIL(FUN_001ca250);

    // Where a sky model's matrices go in VU1's buffer
    extern u32 g_VuSkyData RETAIL(D_00309D3C);
    // The matrix from camera space to a buffer of half the target's size, the screen's middle at its middle
    void HalfSizeScreenMatrix(const RenderTargetDescription* target, Matrix4x4* matrix) RETAIL(FUN_001a2578);
    // The sky drawn first (instead of clearing the frame): the half-size buffer set up in the first bucket, each model's parts
    // around the camera into it (the sky's shaders take their matrices from the buffer's own place), and the buffer drawn over
    // the screen two pixels for one, with the depth written
    void DrawSky(Sky* sky, RenderView* view) RETAIL(FUN_001ba350);
    void StartSkyBuffer(Sky* sky) RETAIL(FUN_001ba488);
    void DrawSkyModel(RigidModel* model, RenderView* view, const Matrix4x4* toScreen, const Matrix4x4* toCamera)
        RETAIL(FUN_001bf3a0);
    void FinishSky(Sky* sky) RETAIL(FUN_001ba6c0);
    u8* FUN_001c1f90(const RigidModel* model, u8* packet, const Matrix4x4* toScreen, const Matrix4x4* toCamera, u32 buffer);
    // A sky material's set-up: RenderMaterial without the CALL and the parameter entries (the bucket's next material makes its
    // own CALL)
    u8* RenderSkyMaterial(Material* material, u8* packet, u32 mode) RETAIL(FUN_001bb5e8);
}

inline DmaChain& ChainOf(const RenderBucket& bucket)
{
    return g_DmaChains[bucket.chain];
}

// A packet goes at the end of the writer's chain, which its last tag is pointed at
inline u32* BeginPacket(RenderBucket& writer)
{
    auto* at = reinterpret_cast<u32*>(ChainOf(writer).next);
    writer.last[1] = Address(at);
    return at;
}

// A packet put in before the writer's last tag (writers started with two), where the one before it points
inline u32* BeginInsertedPacket(RenderBucket& writer)
{
    auto* at = reinterpret_cast<u32*>(ChainOf(writer).next);
    writer.insertion[1] = Address(at);
    return at;
}

// The inserted packet ends at end: a "next" tag after it, to the last tag, takes the next one
inline void EndInsertedPacket(RenderBucket& writer, u8* end)
{
    DmaChain& chain = ChainOf(writer);
    chain.next = end;
    auto* next = reinterpret_cast<u32*>(chain.next);
    writer.insertion = next;
    next[0] = NextTag;
    next[1] = Address(writer.last);
    next[2] = 0;
    next[3] = 0;
    chain.next += 0x10;
}

// The packet ends at end: a "next" tag after it becomes the writer's last
inline void EndPacket(RenderBucket& writer, u8* end)
{
    DmaChain& chain = ChainOf(writer);
    chain.next = end;
    auto* next = reinterpret_cast<u32*>(chain.next);
    writer.last = next;
    next[0] = NextTag;
    next[1] = 0;
    next[2] = 0;
    next[3] = 0;
    chain.next += 0x10;
}

// The material in the frame's list (once): its draws go into its bucket at the frame's end
inline void ListMaterial(Material* material)
{
    if (material->listed == 0)
    {
        material->listed = 1;
        g_RenderedMaterials[g_RenderedMaterialCount++] = material;
    }
}

// Where the view's camera is (its node brought up to date first)
inline Vector4 CameraPosition(const RenderView* view)
{
    Reference* reference = view->cameraObject;
    u8* camera = reference != nullptr ? reinterpret_cast<u8*>(reference->object) : nullptr;
    void* node = *reinterpret_cast<void**>(camera + 8);
    RotateAndTranslate(node);
    return *reinterpret_cast<const Vector4*>(static_cast<u8*>(node) + 0x30);
}
