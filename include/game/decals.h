#pragma once

#include "abi.h"
#include "common.h"
#include "game/math.h"
#include "game/particles.h"

class Stream;
struct ChunkData;
struct Material;

// The decals (footfalls, ripples): blocks of up to 32 decals of a type drawn by VU1, each decal aged by a VU0 microprogram every
// frame and dropped once its life is over. The default chunk's particle data holds them

// A decal's frame in its block, a unit 32768: its normal (its two directions' cross product) and its second direction
struct DecalFrame
{
    s16 normal[3];
    s16 unused06;
    s16 direction[3];
    s16 unused0E;
};
CHECK_SIZE(DecalFrame, 0x10);

// A decal's life in its place's w: its flags (the descriptor's 6 bits) and the frames it has left
union DecalLife
{
    u32 value;
    struct
    {
        u32 flags : 6;
        s32 frames : 26;
    };
};
CHECK_SIZE(DecalLife, 4);

// A decal's place in its block: where it is and its life (a float of -1 for no decal)
struct alignas(16) DecalPlace
{
    f32 x;
    f32 y;
    f32 z;
    union
    {
        DecalLife life;
        f32 w;
    };
};
CHECK_SIZE(DecalPlace, 0x10);

// A block of up to 32 decals of a type and key (0x6F0 bytes): how many it has, its key (the draw list and material, -1 free), its
// index in the pool, the next block of its type and the next of the frame's draw list; its decals' frames and variants; then the
// packet VU1 draws them from: UNPACKs of the decals' places, two sets of four half words the VU0 microprogram works out of their
// sizes (the first set's fourth 0xBA for none), and their colours, and an MSCNT
struct DecalBlock
{
    static constexpr u32 MostDecals = 32;

    s16 count;
    s16 key;
    s16 index;
    s16 unused06;
    DecalBlock* next;
    DecalBlock* drawNext;
    DecalFrame frames[MostDecals];
    s8 variants[MostDecals];
    u32 placesUnpack[4];
    DecalPlace places[MostDecals];
    u32 sizesUnpack[4];
    s16 sizes[MostDecals][4];
    s16 moreSizes[MostDecals][4];
    u32 coloursUnpack[4];
    // Rgba
    u32 colours[MostDecals];
    u32 start[4];
};
CHECK_OFFSET(DecalBlock, variants, 0x210);
CHECK_OFFSET(DecalBlock, places, 0x240);
CHECK_OFFSET(DecalBlock, sizes, 0x450);
CHECK_OFFSET(DecalBlock, colours, 0x660);
CHECK_SIZE(DecalBlock, 0x6F0);

// A variant of a decal type: four colour keys (alpha in w) and four size keys (the time in w, the first's the life in seconds)
struct DecalVariant
{
    f32 colours[4][4];
    f32 sizes[4][4];
};
CHECK_SIZE(DecalVariant, 0x80);

// A decal type as the default particle data has it (0x890 bytes): the DMA tag and UNPACK that send its variants to VU0, 15 variants,
// how many it has and their names
struct DecalType
{
    static constexpr u32 MostVariants = 15;

    u32 tag[4];
    DecalVariant variants[MostVariants];
    s32 variantCount;
    u8 names[0x890 - 0x794];
};
CHECK_OFFSET(DecalType, variantCount, 0x790);
CHECK_SIZE(DecalType, 0x890);

// The packet of the decal types' UV rectangles VU1 gets (TT Lab's DecalUvPacket): a GIF tag the game writes (its first word and
// its registers' two words), then the rectangles
struct alignas(16) DecalUvPacket
{
    u64 gifTag;
    u32 gifRegisters[2];
    Vector4 rectangles[0x40];
};
CHECK_SIZE(DecalUvPacket, 0x410);

// How far footfalls and footprints stand above the ground they're placed on
constexpr f32 FootprintLift = Rounded(0.09);

// The decals of the default particle data (0xE320 bytes): the 32 blocks, the texture page they draw with (its blend modes' materials
// are the four keys'), the packet of the types' UV rectangles VU1 gets with the count of types (and three words read with them,
// never used), the types (16 at most, read when the tools' pointers say so), each type's blocks, the free blocks' stack (an index
// and each block's next), the frame's draw lists by key, the chunk the decals' places are in (the camera's when they were last
// aged) and how many decals there are
struct DecalData
{
    static constexpr u32 BlockCount = 32;
    static constexpr u32 MostTypes = 16;
    // The keys: the page's blend modes
    static constexpr u32 KeyCount = 4;

    DecalBlock blocks[BlockCount];
    ParticlePage page;
    u32 unusedDE1C;
    DecalUvPacket uvPacket;
    u32 typeCount;
    u32 unusedE234[3];
    DecalType* types[MostTypes];
    DecalBlock* typeBlocks[MostTypes];
    s16 freeBlock;
    s16 nextFree[BlockCount];
    s16 unusedE302;
    DecalBlock* drawLists[KeyCount];
    ChunkData* chunk;
    s32 count;
    u32 unusedE31C;
};
CHECK_OFFSET(DecalData, page, 0xDE00);
CHECK_OFFSET(DecalData, uvPacket, 0xDE20);
CHECK_OFFSET(DecalData, typeCount, 0xE230);
CHECK_OFFSET(DecalData, types, 0xE240);
CHECK_OFFSET(DecalData, freeBlock, 0xE2C0);
CHECK_OFFSET(DecalData, drawLists, 0xE304);
CHECK_OFFSET(DecalData, count, 0xE318);
CHECK_SIZE(DecalData, 0xE320);

// A decal to add: its place, its normal and second direction, its variant, type, flags (6 bits), key and the chunk whose space
// they're in
struct DecalDescriptor
{
    Vector4 place;
    Vector4 normal;
    Vector4 direction;
    s32 variant;
    s32 type;
    s32 flags;
    s32 key;
    ChunkData* chunk;
    u32 unused44[3];
};
CHECK_OFFSET(DecalDescriptor, chunk, 0x40);

extern "C"
{
    // The default particle data's decals, and an int their section starts with nothing reads
    extern DecalData g_DecalData RETAIL(G_DefaultParticleData__3240a0);
    extern u32 g_DecalUnusedInt RETAIL(DecalUnusedInt);
    // Every block made empty and free (their packets' UNPACKs and MSCNT written), no types, no decals
    void InitDecalPool(DecalData* decals) RETAIL(InitDecalPool);
    // A block's decals all none
    void ClearDecalBlock(DecalBlock* block) RETAIL(FUN_001b9830);
    // A block of a type and key with room for a decal (taken from the free ones and put first in the type's list when none has):
    // none when every block is taken
    DecalBlock* AllocDecalBlock(DecalData* decals, s16 key, s16 type) RETAIL(AllocDecalBlock);
    // A block back on the free stack
    void FreeDecalBlock(DecalData* decals, DecalBlock* block) RETAIL(FreeDecalBlock);
    // The types let go of and every block emptied and free (their keys and links as they were), no decals
    void ResetDecalPool(DecalData* decals) RETAIL(ResetDecalPool);
    // Every type's blocks emptied and freed, no decals, no chunk
    void ClearDecals(DecalData* decals) RETAIL(FUN_001ae810);
    // The decals' section of the default particle data: the unused int, the UV packet (its GIF tag written), the tools' type
    // pointers and the types they say there are
    void ReadDecalData(DecalData* decals, Stream* stream) RETAIL(ReadDecalData);
    // The decals' page read from the default chunk's stream, or loaded from its startup file with two types' UV rectangles (the
    // decal page's top and bottom halves), or just those
    void ReadDecalPage(DecalData* decals, Stream* stream) RETAIL(FUN_001b9a08);
    void LoadDecals(DecalData* decals, const char* path) RETAIL(FUN_001b9958);
    void SetDefaultDecalTypes(DecalData* decals) RETAIL(FUN_001b98d8);
    // A descriptor's directions made unit, the second made perpendicular to the first when they're more than 0.001 along each other
    // (the first's cross product with a random vector when they're more than 0.99 along each other)
    void OrthonormalizeDecalFrame(DecalDescriptor* decal) RETAIL(OrthonormalizeDecalFrame);
    // A decal's frame in its block from its descriptor (made orthonormal first)
    void SetDecalFrame(DecalDescriptor* decal, DecalBlock* block, s32 slot) RETAIL(SetDecalFrame);
    // A decal added (its place and directions taken into the camera's chunk's space from its own; none while the decals are in
    // another chunk than the camera's, or when no block has room)
    void AddDecal(DecalData* decals, DecalDescriptor* decal) RETAIL(AddDecal);
    // A decal of type 0, key 0 and variant 0 at a matrix's place, 0.09 above it, its normal and direction the matrix's second and
    // third rows, in a chunk
    void AddDecalFromDescriptor(const Matrix4x4* frame, ChunkData* chunk) RETAIL(AddDecalFromDescriptor);
    // The chunk the camera is in (none without a camera)
    ChunkData* CameraChunk() RETAIL(FUN_001b9870);
    // The decals' view loaded for the frame (the camera's chunk, the decals taken there from the chunk they were in) and that chunk
    // made theirs. Whether there's a camera
    s32 UploadDecalCameraToVU0(DecalData* decals) RETAIL(UploadDecalCameraToVU0);
    // The decals' frame of a length (seconds): each type's blocks' decals aged (a frame of their life less unless the frame took
    // no time), the ones whose life is over dropped and the rest moved up, empty blocks freed and the others put in the draw list of
    // their key. A block's decals all age with its first decal's variant. Whether there were decals and a camera
    s32 UpdateDecalsVU0(DecalData* decals, f32 delta) RETAIL_N32(UpdateDecalsVU0);
}
