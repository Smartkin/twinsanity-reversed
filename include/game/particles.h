#pragma once

#include "abi.h"
#include "common.h"
#include "game/math.h"

class Stream;
struct ChunkData;

// The game's particles (TT Lab's TwinParticleSystem and TwinParticleEmitter; Ghidra Stuff/Particles_RE has the notes): systems
// read from a chunk's particle section (300 at most across the loaded chunks, index 0 none), emitters placing them (0x300), and
// the emitters' runtimes (384) the frames run. Times are frames of 1/60 s, angles 65536ths of a turn

// The generators a system's genSort picks (TT Lab's GenSortType; the retail data uses 0, 6, 7, 8, 9 and 11)
enum ParticleGenSort : u8
{
    GenSortBox = 0,
    GenSortBox2 = 1,
    GenSortRanges = 2,
    GenSortLine = 3,
    GenSortReuse = 4,
    GenSortRangesRandomLife = 5,
    GenSortRadial = 6,
    GenSortRadialRotor = 7,
    GenSortSpheroid = 8,
    GenSortBounce = 9,
    GenSortBounceXZ = 10,
    GenSortSphere = 11,
    GenSortStar = 12,
};

// A system's blend mode, the material of its page it draws with (TT Lab's ParticleBlendModes): additive, subtractive, the page
// material's own first shader and cutout (4-6 take the next page's IDs for a material), and 7 the hexagons distorting the frame
enum ParticleBlendMode : u8
{
    BlendAdditive = 0,
    BlendSubtractive = 1,
    BlendPageMaterial = 2,
    BlendCutout = 3,
    BlendDistortion = 7,
};

// A system's draw list: list 0 is drawn (its blend modes 3, 2, 1, 0 in turn), list 1 never, list 2 the distorting hexagons' (every
// system of mode 7 goes there)
enum ParticleDrawList : u8
{
    DrawListParticles = 0,
    DrawListDistortion = 2,
};
constexpr u32 ParticleDrawListCount = 4;

// The emitter runtimes a system has slots for (nothing ever puts one in them: they stay -1)
constexpr u32 SystemEmitterSlots = 4;

// A key of a curve over a particle's life: the share of the life and the value. A curve has 8
struct ParticleKey
{
    f32 time;
    f32 value;
};
constexpr u32 ParticleCurveKeys = 8;

// A key of the colour curve: the share of the life and the colour (128 leaves the texture as it is)
struct ParticleColourKey
{
    f32 time;
    f32 red;
    f32 green;
    f32 blue;
};
CHECK_SIZE(ParticleColourKey, 0x10);

// A particle system's ID of none (a trail's, a decal's, a controller's, a script's)
constexpr u16 NoParticleSystem = 0xFFFF;

// A loaded system: its name, the section it came from (its kind, which nothing reads, and its slot), its rate (particles a
// frame, negative one every that many frames), the particles it keeps at most (worked out at load), its on/off cycle (frames,
// with random extras), its generator (ParticleGenSort) and velocity rule, its blend mode (ParticleBlendMode), the camera
// distances it runs and is drawn within, its velocity and the random spreads of its starts and velocities (box: per axis;
// radial: radius/speed, yaw and tilt), the ranges' scales and bases (generators 2-5), its gravity and life (seconds), a texture
// animation nothing reads, the jibber, its colour, alpha, size and rotation curves, the distortion, its texture rectangle (pixels
// + 2^19), the table of 64 life steps the renderer bakes, the collision spheres' radius curve and count, the draw list
// (ParticleDrawList), its emitter slots, its ghosts (copies spawning later), the radial ramp, the star's points and ratio, its
// texture page, its scale and its culling box
struct ParticleSystem
{
    char name[16];
    u8 unused10;
    u8 sectionSlot;
    s16 genRate;
    u16 maxParticles;
    u16 timingOffset;
    u16 onTime;
    u16 onTimeRandom;
    u16 offTime;
    u16 offTimeRandom;
    u8 genSort;
    u8 genCode;
    u8 blendMode;
    u8 unusedByte;
    f32 unusedFloat1;
    f32 cutOnRadius;
    f32 cutOffRadius;
    f32 drawCutOff;
    f32 unusedFloat5;
    f32 unusedFloat6;
    f32 velocity;
    f32 randomEmit[3];
    f32 randomStart[3];
    f32 startRandomScale[3];
    f32 startBase[3];
    f32 velocityRandomScale[3];
    f32 velocityBase[3];
    f32 gravity;
    f32 lifeTime;
    u16 textureFrameCount;
    u8 textureFrameStart;
    u8 textureFrameHold;
    f32 textureFrameRate;
    f32 jibberXFreq;
    f32 jibberXAmp;
    f32 jibberYFreq;
    f32 jibberYAmp;
    ParticleColourKey colourKeys[ParticleCurveKeys];
    ParticleKey alphaKeys[ParticleCurveKeys];
    f32 distortionX;
    f32 distortionY;
    f32 minSize;
    f32 maxSize;
    ParticleKey widthKeys[ParticleCurveKeys];
    ParticleKey heightKeys[ParticleCurveKeys];
    f32 minRotation;
    f32 maxRotation;
    ParticleKey rotationKeys[ParticleCurveKeys];
    ParticleKey unusedKeys1[ParticleCurveKeys];
    ParticleKey unusedKeys2[ParticleCurveKeys];
    f32 textureStartX;
    f32 textureStartY;
    f32 textureEndX;
    f32 textureEndY;
    u8* renderTable;
    ParticleKey collisionRadiusKeys[ParticleCurveKeys];
    u8 collisionSpheres;
    u8 drawList;
    s16 emitterSlots[SystemEmitterSlots];
    s16 ghosts;
    s16 starPoints;
    s16 pad322;
    f32 ghostSeparation;
    f32 rampTime;
    f32 starRadiusRatio;
    s32 texturePage;
    f32 scaleFactor;
    u8 unused338[8];
    f32 boundingExtents[4];
};
CHECK_OFFSET(ParticleSystem, genSort, 0x20);
CHECK_OFFSET(ParticleSystem, colourKeys, 0xA8);
CHECK_OFFSET(ParticleSystem, alphaKeys, 0x128);
CHECK_OFFSET(ParticleSystem, widthKeys, 0x178);
CHECK_OFFSET(ParticleSystem, rotationKeys, 0x200);
CHECK_OFFSET(ParticleSystem, textureStartX, 0x2C0);
CHECK_OFFSET(ParticleSystem, renderTable, 0x2D0);
CHECK_OFFSET(ParticleSystem, collisionSpheres, 0x314);
CHECK_OFFSET(ParticleSystem, ghosts, 0x31E);
CHECK_OFFSET(ParticleSystem, texturePage, 0x330);
CHECK_OFFSET(ParticleSystem, boundingExtents, 0x340);
CHECK_SIZE(ParticleSystem, 0x350);

// A loaded emitter: where it is, its system's index, its runtime (-1 none, 99999 read but not made), its gravity's and emission's
// turns, its timing offset (frames), its name (its system's), switches nothing reads, the vertical bounce plane's turn about y,
// the bounce plane's offset and the bounce factor, a group nothing reads and the section slot it came from
struct ParticleEmitter
{
    f32 position[3];
    s32 system;
    s32 runtime;
    s16 gravityTilt;
    s16 gravityYaw;
    s16 emitTilt;
    s16 emitYaw;
    s16 emitRoll;
    s16 pad1E;
    s32 timingOffset;
    char name[16];
    s32 switchType;
    s32 switchId;
    f32 switchValue;
    s16 unusedShort;
    s16 bouncePlaneAngle;
    f32 planeOffset;
    f32 bounceFactor;
    s16 groupId;
    u8 sectionSlot;
    u8 pad4F;
};
CHECK_OFFSET(ParticleEmitter, name, 0x24);
CHECK_OFFSET(ParticleEmitter, bounceFactor, 0x48);
CHECK_SIZE(ParticleEmitter, 0x50);

// A collision sphere of an emitter: its velocity (along the emission's up) and its age (-1 none)
struct CollisionSphere
{
    f32 velocity[3];
    f32 age;
};

// The blocks of particles a runtime can have, and the collision spheres it has room for
constexpr s16 MaxEmitterBlocks = 32;
constexpr u32 EmitterCollisionSpheres = 8;

struct EmitterRuntime;
struct ParticleRecord;

// A generator: a runtime's next particle made (its start, velocity and spawn time written into the next slot of its blocks, its
// ghosts after it). Returns it (none when the generator can't make the system's particles)
using ParticleGenerator = ParticleRecord* (*)(EmitterRuntime* runtime, ParticleSystem* system);
// A velocity rule: a new particle's velocity made from its start
using ParticleVelocityRule = void (*)(EmitterRuntime* runtime, ParticleSystem* system, ParticleRecord* record);

// An emitter's runtime (0x200 bytes): its blocks of particles, its gravity's and emission's matrices, the blocks it has, its
// system, whether it runs, the particles its blocks hold, the slot the next particle goes in, the blocks it wants and the particles
// it keeps at most, where it is, its generator and velocity rule, the next and previous runtimes of its wheel slot, the rotor's
// yaw and tilt (generator 7), the on/off cycles its starter left it (0: it runs on), the frames left of its on time and until it's
// run, its collision spheres (the next one and the frames until it), the bounce, its state, whether its draw entries are in a list,
// whether its emission keeps its translation, its chunk, and the time it was made
struct EmitterRuntime
{
    // Its states: free or the game's own (KillParticles lets those go, it keeps the states from 100 up), or a chunk's (a section's
    // emitter, or one moved into another chunk)
    enum State : s16
    {
        StateLoose = -1,
        StateKeptFrom = 100,
        StateOfChunk = 0x65,
    };

    // How the camera turns it on and off: never, by its distance (the only one retail sets) or always on
    enum CameraSwitch : u8
    {
        CameraNeverOn = 0,
        CameraByDistance = 1,
        CameraAlwaysOn = 2,
    };

    u8* blocks[MaxEmitterBlocks];
    Matrix4x4 gravityMatrix;
    Matrix4x4 emitMatrix;
    s16 blockCount;
    s16 system;
    s16 enabled;
    s16 capacity;
    s16 unused108;
    s16 nextSlot;
    s16 blocksWanted;
    s16 maxParticles;
    f32 position[3];
    // Added to every particle's start and velocity (nothing sets them: zero)
    f32 startOffset[3];
    f32 velocityOffset[3];
    ParticleGenerator generator;
    ParticleVelocityRule velocityRule;
    EmitterRuntime* next;
    EmitterRuntime* previous;
    s16 rotorYaw;
    s16 rotorTilt;
    u32 cyclesLeft;
    s16 onTimeLeft;
    s16 phase;
    CollisionSphere spheres[EmitterCollisionSpheres];
    s16 nextSphere;
    s16 sphereCountdown;
    // The emitter's bounce: a short nothing reads, the vertical plane's turn about y, the plane's offset and the factor
    s16 unusedShort;
    s16 bouncePlaneAngle;
    f32 planeOffset;
    f32 bounceFactor;
    s16 state;
    u8 cameraSwitch;
    u8 listsDraws;
    u8 keepsEmitTranslation;
    u8 unused1E5[3];
    ChunkData* chunk;
    // The camera's distance from it when it was last run (far away when its chunk wasn't drawn that frame)
    f32 cameraDistance;
    f32 creationTime;
    u8 unused1F4[0x200 - 0x1F4];
};
CHECK_OFFSET(EmitterRuntime, gravityMatrix, 0x80);
CHECK_OFFSET(EmitterRuntime, blockCount, 0x100);
CHECK_OFFSET(EmitterRuntime, maxParticles, 0x10E);
CHECK_OFFSET(EmitterRuntime, generator, 0x134);
CHECK_OFFSET(EmitterRuntime, next, 0x13C);
CHECK_OFFSET(EmitterRuntime, phase, 0x14E);
CHECK_OFFSET(EmitterRuntime, spheres, 0x150);
CHECK_OFFSET(EmitterRuntime, nextSphere, 0x1D0);
CHECK_OFFSET(EmitterRuntime, bounceFactor, 0x1DC);
CHECK_OFFSET(EmitterRuntime, keepsEmitTranslation, 0x1E4);
CHECK_OFFSET(EmitterRuntime, chunk, 0x1E8);
CHECK_SIZE(EmitterRuntime, 0x200);

// The frames of the particles' wheels: the runtimes and the events due in each of the frames to come
constexpr u32 ParticleWheelFrames = 32;

// An event of a block, due on a frame of the wheel: the block, the frames still to wait past that frame, its kind, the runtime (a
// block taken from it), the next event of its frame, and a bounce's particle, time (seconds into its life), system, vertical
// plane's turn about y, plane offset and factor
struct ParticleEvent
{
    // A block of particles or of hexagons off its draw list (then its chain ended, then made free), a block taken from its runtime
    // (nothing queues it), a particle's bounce off the plane and off the vertical plane
    enum Kind : s32
    {
        DropParticleDraw = 0,
        BlockTaken = 1,
        EndParticleChain = 2,
        FreeParticleBlock = 3,
        PlaneBounce = 5,
        WallBounce = 6,
        DropHexagonDraw = 7,
        FreeHexagonBlock = 8,
        EndHexagonChain = 9,
    };

    u8* block;
    s32 delay;
    s32 kind;
    EmitterRuntime* runtime;
    ParticleEvent* next;
    s32 record;
    f32 time;
    s16 system;
    s16 unused1E;
    s16 planeAngle;
    s16 pad22;
    f32 planeOffset;
    f32 bounceFactor;
};
CHECK_OFFSET(ParticleEvent, record, 0x14);
CHECK_OFFSET(ParticleEvent, planeAngle, 0x20);
CHECK_SIZE(ParticleEvent, 0x2C);

// A block's entry of a draw list: the block (its chain's first), its system, the runtime drawing it (none once it has let go of
// the block, which then draws until its particles are gone with what the runtime had: its gravity's matrix, its place, its chunk,
// its texture page and its emission's flag), and the previous and next entries of its list
struct ParticleDrawEntry
{
    u8* block;
    ParticleSystem* system;
    EmitterRuntime* owner;
    u32 unused0C;
    Matrix4x4 matrix;
    f32 position[3];
    ParticleDrawEntry* previous;
    ParticleDrawEntry* next;
    ChunkData* chunk;
    s16 texturePage;
    u8 keepsEmitTranslation;
    u8 unused6B[5];
};
CHECK_OFFSET(ParticleDrawEntry, matrix, 0x10);
CHECK_OFFSET(ParticleDrawEntry, previous, 0x5C);
CHECK_OFFSET(ParticleDrawEntry, chunk, 0x64);
CHECK_SIZE(ParticleDrawEntry, 0x70);

// A particle of a block (after the block's 0x20 bytes of packet): its start, the time it was made, its velocity and 64 over its
// life (32768 for none)
struct ParticleRecord
{
    f32 start[3];
    f32 spawnTime;
    f32 velocity[3];
    f32 lifeScale;
};
CHECK_SIZE(ParticleRecord, 0x20);
constexpr u32 ParticleBlockHeader = 0x20;

constexpr u32 MaxParticleSystems = 300;
constexpr u32 MaxParticleEmitters = 0x300;
constexpr u32 MaxEmitterRuntimes = 0x180;
constexpr u32 ParticleSectionSlots = 16;
// The sections' kinds: the default chunk's (its systems) and a level's (its systems and emitters)
enum ParticleSectionKind : s8
{
    SectionDefault = 0,
    SectionLevel = 1,
};
// The section slots: the default particles' and the levels' first (2-15)
constexpr s8 DefaultParticleSlot = 0;
constexpr s8 FirstLevelParticleSlot = 2;
// An emitter read but not made yet
constexpr s32 EmitterNotMade = 99999;

// A texture page the particles (and the decals) draw with (0x1C bytes): its texture's and material's IDs, the material, and the
// platform's materials of its blend modes 0-3 (additive, subtractive, the page material's own, cutout)
struct ParticlePage
{
    u32 textureId;
    u32 materialId;
    struct MaterialResource* material;
    struct Material* materials[4];
};
CHECK_SIZE(ParticlePage, 0x1C);
constexpr u32 ParticlePageCount = 3;

extern "C"
{
    // The systems (index 0 none) and the table of the loaded ones, how many are loaded
    extern ParticleSystem g_ParticleSystems[MaxParticleSystems] RETAIL(D_00332540);
    extern ParticleSystem* g_LoadedParticleSystems[MaxParticleSystems] RETAIL(G_NullParticlePtr);
    extern s32 g_LoadedParticleSystemCount RETAIL(G_ParticlesAmountLoaded_);
    // The emitters and how many are loaded, and how many named a system that isn't loaded
    extern ParticleEmitter g_ParticleEmitters[MaxParticleEmitters] RETAIL(D_003A1520);
    extern s32 g_ParticleEmitterCount RETAIL(G_ParticleInstancesAmount_);
    extern s32 g_UnfoundParticleSystems RETAIL(D_00309C9C);
    extern EmitterRuntime g_EmitterRuntimes[MaxEmitterRuntimes] RETAIL(ParticleEmitterRuntimes);
    // The section slots in use and their chunks (slot 0 the default particles, 2-15 the levels')
    extern s32 g_ParticleSlotsUsed[ParticleSectionSlots] RETAIL(D_003A14A0);
    extern ChunkData* g_ParticleSlotChunks[ParticleSectionSlots] RETAIL(D_003B0520);
    // The stream particle sections are read from, and whether it was set; two streams and a disk file the reading could keep
    // (nothing makes them: the file is -1 from the start, the streams none)
    extern Stream* g_ParticleReader RETAIL(G_ParticleBinReader);
    extern u8 g_ParticleReaderSet RETAIL(D_00309BC0);
    extern Stream* g_ParticleCopyStream RETAIL(D_00309BB4);
    extern Stream* g_ParticleOtherStream RETAIL(D_00309BB8);
    extern s32 g_ParticleSectionFile RETAIL(D_0030A800);

    // The reader's values: a block of bytes, a float, a word, a half word and a byte (both sign extended)
    void ReadParticleBytes(void* buffer, u32 size) RETAIL(ReadBytesIntoMemWithGlobalBinReader);
    f32 ReadParticleFloat() RETAIL(ReadFloatFromGlobalBinReader);
    u32 ReadParticleWord() RETAIL(ReadUIntFromGlobalBinReader);
    s32 ReadParticleHalf() RETAIL(ReadUShortFromGlobalBinReader);
    s32 ReadParticleByte() RETAIL(ReadByteFromGlobalBinReader);
    // The stream the sections are read from set (returns 0x65, which nothing reads)
    u32 SetParticleReader(Stream* stream) RETAIL(SetParticleDataBinReader);

    // A system read from a section of a version and kind, its particles at most worked out (resizing its running emitters'
    // blocks), its culling box for versions before 0x1E
    void ReadParticleSystem(ParticleSystem* system, s32 version, s32 kind) RETAIL(ReadParticleSystem);
    void ComputeParticleMaxCount(ParticleSystem* system) RETAIL(ComputeParticleMaxCount);
    void ComputeParticleBoundingExtents(ParticleSystem* system) RETAIL(ComputeParticleBoundingExtents);
    // A section of a kind (ParticleSectionKind; a level's: slot 2 or the first free from 3, -1 none; the others slot 0) read with
    // its systems and, for kinds 1 and 2, its emitters, which are made. Returns the slot
    s32 ReadParticleSection(s8 kind, ChunkData* chunk) RETAIL(ReadMainParticleSection);
    // The frames' particle time, count and length (0.02 s on PAL, 1/60 s on NTSC)
    extern f32 g_ParticleTime RETAIL(ParticleTime);
    extern s32 g_ParticleFrameCounter RETAIL(ParticleFrameCounter);
    extern f32 g_ParticleFrameDelta RETAIL(ParticleFrameDelta);
    // Particles were set up for the level; the blocks of 32 particles and of 12 hexagons there are
    extern s32 g_ParticlesSetUp RETAIL(D_00309C70);
    extern s32 g_ParticleBlockCount RETAIL(D_00309C68);
    extern s32 g_HexagonBlockCount RETAIL(D_00309C6C);
    // The events (two of 0x2C bytes a block) and the stack of free ones, the blocks, and the draw entries (0x70 bytes a block)
    extern ParticleEvent* g_ParticleEvents RETAIL(D_0030A858);
    extern ParticleEvent** g_ParticleEventPool RETAIL(D_0030A874);
    extern s32 g_ParticleEventPoolTop RETAIL(D_0030A878);
    // The events due in each of the wheel's frames to come, the wheel's place, and the draw lists' first entries
    extern ParticleEvent* g_ParticleEventWheel[ParticleWheelFrames] RETAIL(ParticleEventWheel);
    extern s32 g_ParticleEventWheelIndex RETAIL(ParticleEventWheelIndex);
    extern ParticleDrawEntry* g_ParticleDrawLists[ParticleDrawListCount] RETAIL(ParticleDrawLists);
    extern u8** g_ParticleBlocks RETAIL(D_0030A860);
    extern u8** g_HexagonBlocks RETAIL(D_0030A864);
    extern ParticleDrawEntry* g_ParticleDrawEntries RETAIL(D_0030A880);
    // The blocks of particles and of hexagons taken
    extern s32 g_UsedParticleBlocks RETAIL(D_0030A868);
    extern s32 g_UsedHexagonBlocks RETAIL(D_0030A86C);
    // The free runtimes (a stack of their indexes) and how many are taken
    extern s16 g_FreeEmitterRuntimes[MaxEmitterRuntimes] RETAIL(D_003A1110);
    extern s32 g_TakenEmitterRuntimes RETAIL(D_0030A870);
    // The emitters due in each of the wheel's frames to come, and the wheel's place
    extern EmitterRuntime* g_ParticleEmitterWheel[ParticleWheelFrames] RETAIL(ParticleEmitterWheel);
    extern s32 g_ParticleEmitterWheelIndex RETAIL(ParticleEmitterWheelIndex);
    // The distorting hexagons' material (the renderer's)
    extern struct Material g_DistortionMaterial RETAIL(D_00370758);
    // The particles set up for a level with a count of blocks: the tables allocated, the systems' render tables let go, the
    // blocks made, every runtime free, the distortion's material made
    void InitParticleSystemsForLevel(s32 blocks) RETAIL(InitParticleSystemsForLevel);
    // The blocks, the render tables' pool and the events made, every list empty
    void InitParticleBlocks(s32 blocks) RETAIL(FUN_001b3e48);
    // The section slots whose emitters were made
    extern s32 g_ParticleSlotsMade[ParticleSectionSlots] RETAIL(D_003A14E0);
    // The render tables of unloaded systems, kept for the next ones: the pool and where its top is
    extern u8* g_RenderTablePool[] RETAIL(D_003A07D0);
    extern s32 g_RenderTablePoolTop RETAIL(D_0030A84C);
    // The emitters of a slot read but not made made: their runtimes with the gravity's and emission's matrices, the timing and the
    // bounce, in the slot's chunk (the drawn chunk without one)
    void CreateLoadedParticleEmitters(s8 slot) RETAIL(CreateLoadedParticleEmitters);
    // A slot's emitters and systems let go of, the systems' render tables kept in the pool
    void UnloadParticleSection(s8 slot) RETAIL(UnloadParticleSection);
    // A runtime made for a system at a place (one taken when *runtime is -1, -1 none): its timing, generator, matrices and bounce
    // set and it put first in the wheel's current slot
    void CreateParticleEmitter(f32 x, f32 y, f32 z, s32* runtime, s32 system, ChunkData* chunk) RETAIL_N32(CreateParticleEmitter);
    // A runtime's on/off cycle lined up with the frame counter and the offset
    void SetParticleEmitterTiming(s32 runtime, s32 timingOffset) RETAIL(SetParticleEmitterTiming);
    // A runtime let go of, *runtime -1
    void ReleaseParticleEmitter(s32* runtime) RETAIL(ReleaseParticleEmitter);
    // A runtime let go of with its particles left to finish: its blocks drawn on their own until they're gone, then released
    void FreeParticleEmitterBlocks(s32* runtime) RETAIL(FreeParticleEmitterBlocks);
    // A system's draw entries off their lists, their blocks' events dropped and their release queued
    void ForgetParticleSystemDraws(ParticleSystem* system) RETAIL(FUN_001b58a8);
    // A free runtime taken (-1 none)
    s16 AllocParticleEmitterSlot() RETAIL(AllocParticleEmitterSlot);
    // A runtime's blocks dropped: their events, their release queued, its draw entries off their list
    void DropEmitterBlocks(s32* runtime) RETAIL(FUN_001b5400);
    // The wheel slot a runtime is in (none: nullptr), and the runtime taken out of it
    EmitterRuntime** FindEmitterInWheel(EmitterRuntime* runtime) RETAIL(FUN_001b9f70);
    void UnlinkEmitter(EmitterRuntime* runtime, EmitterRuntime** slot) RETAIL(FUN_001b9fd8);
    // A runtime's chunk (the drawn chunk for none) and its gravity's and emission's matrices (the emission's translation kept
    // when it says so; either's translation is its position)
    void SetEmitterChunk(s32 runtime, ChunkData* chunk) RETAIL(FUN_001b9f40);
    void SetEmitterGravity(s32 runtime, const Matrix4x4* matrix) RETAIL(FUN_001b9e48);
    void SetEmitterEmission(s32 runtime, const Matrix4x4* matrix) RETAIL(FUN_001b9ec0);
    // An event added at the end of a frame's list
    void AppendParticleEvent(ParticleEvent* event, ParticleEvent** list) RETAIL(FUN_001b9df8);
    // An emitter made running at a place for a count of on/off cycles (0: on and on), in the drawn chunk for none
    void StartParticleEmitter(s32* runtime, s32 system, const f32* position, u32 cycles, ChunkData* chunk) RETAIL(FUN_001ba018);
    // The runtimes due this frame: their on time counted down (a stopped one looked at again in 50-57 frames), the next on/off
    // cycle when it's up, and the runtime let go of once its cycles are done
    void UpdateParticleEmitterTiming() RETAIL(UpdateParticleEmitterTiming);
    // The runtimes due this frame moved to the frame their phase says (the ones with collision spheres to the next frame), the
    // wheel turned on
    void RescheduleParticleEmitters() RETAIL(RescheduleParticleEmitters);
    // The events due this frame: bounces, draw list drops, blocks taken from runtimes, chains ended and blocks made free; the wheel
    // turned on
    void ProcessParticleBlockEvents() RETAIL(ProcessParticleBlockEvents);
    // The runtimes with collision spheres objects are tested against (listed again every frame as they're due): their systems,
    // the runtimes and how many there are, and the centre of the last sphere looked at
    extern ParticleSystem* g_CollidingSystems[] RETAIL(D_003A0C90);
    extern EmitterRuntime* g_CollidingRuntimes[] RETAIL(D_003A0E90);
    extern s32 g_CollidingCount RETAIL(D_0030A854);
    extern Vector4 g_CollisionSphereCentre RETAIL(D_003A0C80);
    // The runtimes due this frame with collision spheres listed for the test, their spheres aged (none once past the system's
    // life) and, once the countdown is up, a new one sent off at the system's velocity along the emission's up (the life over
    // the spheres apart)
    void UpdateParticleCollisionSpheres() RETAIL(UpdateParticleCollisionSpheres);
    // The first listed runtime with a sphere that a sphere of a radius at a point touches (-1 none). A sphere is where the
    // particles sent off with it are (its velocity turned by the gravity's matrix, falling with the system's gravity), its
    // radius the system's curve at its share of the life; with a vertical scale other than 1 the height between them counts
    // for more or less (the point's sphere squashed by the scale)
    s32 TestParticleCollisionSpheres(const f32* point, f32 radius, f32 verticalScale) RETAIL_N32(TestParticleCollisionSpheres);
    // The test with no vertical scale
    s32 TestParticleCollision(const f32* point, f32 radius) RETAIL_N32(FUN_001ba148);
    // The chunks whose views the frame's particles loaded (the platform's, Platform::Graphics::LoadParticleView: as many as there's
    // room for on the retail stack) and the frame they're for
    struct ParticleViews
    {
        static constexpr u32 MostChunks = 18;

        s32 count;
        u32 frame;
        ChunkData* chunks[MostChunks];
    };
    // Particles run (never off in retail)
    extern s32 g_ParticlesOn RETAIL(D_00309C58);
    // The runtimes due this frame run: on and off by the camera's distance (between the system's cut on and cut off radiuses, in
    // the view of their chunk, loaded the first time this frame), given their blocks when they're on and taking them back when
    // they're off, and their particles made (the system's rate a frame, or one every that many frames when it's negative).
    // Returns how many made any
    s32 GenerateParticles(ParticleViews* views) RETAIL(GenerateParticles);
    // A draw list's blocks drawn, a blend mode at a time in the list's order (list 2: the distorting hexagons; the others 3, 2, 1, 0),
    // each in its chunk's view (loaded the first time this frame; none for a chunk not drawn this frame) unless it's out of it or
    // past its system's draw distance. The frozen flag is never read. Returns how many entries it went through
    s32 DrawParticleList(s32 frozen, s32 list, ParticleViews* views) RETAIL(DrawParticleList);
    // The blend modes each draw list draws (list 2's, the others') and how many
    extern s32 g_ParticleModeCount RETAIL(D_00309C88);
    extern const s32 g_ParticleModeOrder[4] RETAIL(D_002E78E8);
    extern s32 g_DistortionModeCount RETAIL(D_00309C94);
    extern const s32 g_DistortionModeOrder[1] RETAIL(D_00309C90);
    // The default chunk's three particle texture pages (each made by the platform, ParticlePage) and a page's additive material
    extern ParticlePage g_ParticlePages[ParticlePageCount] RETAIL(D_00370700);
    struct Material* ParticlePageMaterial(u32 page) RETAIL(FUN_001b9d38);
    // A page read from a chunk's stream
    void ReadParticlePageAt(u32 page, Stream* stream) RETAIL(FUN_001b9d00);
    // The pages loaded from their startup files (startup\<name><page>.ptc), then the systems' blocks made
    void LoadParticlePages(const char* name, s32 blocks) RETAIL(FUN_001b9d58);
    // The pages (and 0x80 blocks of particles) and the decals' page and types loaded at start-up. Returns 1
    s32 LoadParticles(const char* name) RETAIL(LoadParticles_);
    // The 0x80 blocks of particles made and the decals given their default types, when the default chunk's RM2 brings the pages
    // (no startup files read). Returns 1
    s32 SetUpDefaultParticles() RETAIL(FUN_0025cf88);
    // The particle section of an RM2: the default chunk's three pages, systems (it has no emitters), decals' page and types (every
    // section unloaded first), or a level's systems and emitters, from the RM2's stream. A path of each page's file is made and
    // dropped
    void ReadParticleData(class Rm2Reader* reader, Stream* stream) RETAIL(ReadParticleData);
    // The static constructor: the particles started (InitParticles(1, 0xFFFF))
    void ParticlesStaticInit() RETAIL(FUN_001ba330);
    // The particles' frame of a length (seconds; none of their time passes while the game's frozen): the collision spheres, the
    // runtimes' blocks, their particles made, their timing and the blocks' events, then the draw lists 0 and 2 drawn. The particle
    // time starts again at 0 when nothing ran or drew
    void UpdateAndDrawParticles(s32 frozen, f32 delta) RETAIL_N32(UpdateAndDrawParticles);
    // The generators by genSort and the velocity rules by genCode
    extern const ParticleGenerator g_ParticleGenerators[] RETAIL(ParticleGeneratorTable);
    extern const ParticleVelocityRule g_ParticleVelocityRules[] RETAIL(ParticleGenCodeTable);
    // The generators' random numbers
    extern u64 g_ParticleRandom RETAIL(ParticleRandomSeed);
    // A system's render table of its curves' 64 steps made
    void BuildParticleRenderTable(ParticleSystem* system) RETAIL(BuildParticleRenderTable);
    // The generators (TT Lab's GenSortType), turned by the emission's matrix. Box: a start up to the random start's either way
    // along each axis, a velocity of the system's up plus up to the random emit's either way (the second box generator is the
    // first)
    ParticleRecord* GenParticle_Box(EmitterRuntime* runtime, ParticleSystem* system) RETAIL(GenParticle_Box);
    ParticleRecord* GenParticle_Box2(EmitterRuntime* runtime, ParticleSystem* system) RETAIL(GenParticle_Box2);
    // Radial: a start on a ring or cone (random start: radius, which the ramp grows from 0 over its time, yaw and tilt; the random
    // emit: the speed's, the yaw's and the tilt's spreads), going out from the middle; the rotor's the same with its yaw and tilt
    // turning on by the spreads after each particle instead of random ones. Neither makes hexagons
    ParticleRecord* GenParticle_Radial(EmitterRuntime* runtime, ParticleSystem* system) RETAIL(GenParticle_Radial);
    ParticleRecord* GenParticle_RadialRotor(EmitterRuntime* runtime, ParticleSystem* system) RETAIL(GenParticle_RadialRotor);
    // Spheroid: a start in a disc (the yaw it means to pick is always 0) scaled by the random start, the box's velocity
    ParticleRecord* GenParticle_Spheroid(EmitterRuntime* runtime, ParticleSystem* system) RETAIL(GenParticle_Spheroid);
    // Bounce: the box's particle with its bounce off the emitter's plane queued (no ghosts, no hexagons)
    ParticleRecord* GenParticle_Bounce(EmitterRuntime* runtime, ParticleSystem* system) RETAIL(GenParticle_Bounce);
    // Ranges: the start and velocity per axis the C library's rand (up to 2^31) times the range's scale past its base, unturned
    // and born at the frame's start; the second gives each a life up to 1/0.7 longer. Line: a start up to 0.3 along a fixed line
    // through the middle, a velocity 1.5 times out along it and 1 to 2.5 along another, the heights the ranges'. Reuse: the slot's
    // last start's x and z kept and doubled into the velocity, the heights the ranges'. None of them makes ghosts or hexagons
    ParticleRecord* GenParticle_Ranges(EmitterRuntime* runtime, ParticleSystem* system) RETAIL(GenParticle_Ranges);
    ParticleRecord* GenParticle_RangesRandomLife(EmitterRuntime* runtime, ParticleSystem* system) RETAIL(GenParticle_RangesRandomLife);
    ParticleRecord* GenParticle_Line(EmitterRuntime* runtime, ParticleSystem* system) RETAIL(GenParticle_Line);
    ParticleRecord* GenParticle_Reuse(EmitterRuntime* runtime, ParticleSystem* system) RETAIL(GenParticle_Reuse);
    // Bounce off the vertical plane: the box's particle with its bounce queued for when its path over its life crosses the
    // emitter's vertical plane (the x axis turned about y by the plane's angle, the offset along it)
    ParticleRecord* GenParticle_BounceXZ(EmitterRuntime* runtime, ParticleSystem* system) RETAIL(GenParticle_BounceXZ);
    // Star: the radial generator's start pulled in between the points (a star of the system's points, the radius between the
    // points the ratio of the radius at them)
    ParticleRecord* GenParticle_Star(EmitterRuntime* runtime, ParticleSystem* system) RETAIL(GenParticle_Star);
    // Sphere: a start on a sphere of the random start's radius, even over its area between two tilts (random start's tilt less
    // and plus the random emit's), its yaw the random start's plus up to the random emit's either way, going out from the middle
    ParticleRecord* GenParticle_Sphere(EmitterRuntime* runtime, ParticleSystem* system) RETAIL(GenParticle_Sphere);
    // The velocity rules (TT Lab's GenCode, always 0 in the retail data): the velocity's x and z twice the start's, minus it, 4
    // and 16 times it; the start pulled in towards the middle (0.6 of x and z off the velocity's, 0.4 of the flat distance off
    // its y) with a life up to 1.43 times longer; the whole velocity 5.4 times the start
    void GenCode1_VelocityXZ2xStart(EmitterRuntime* runtime, ParticleSystem* system, ParticleRecord* record) RETAIL(GenCode1_VelocityXZ2xStart);
    void GenCode2_VelocityXZMinusStart(EmitterRuntime* runtime, ParticleSystem* system, ParticleRecord* record) RETAIL(GenCode2_VelocityXZMinusStart);
    void GenCode3_VelocityXZ4xStart(EmitterRuntime* runtime, ParticleSystem* system, ParticleRecord* record) RETAIL(GenCode3_VelocityXZ4xStart);
    void GenCode4_VelocityXZ16xStart(EmitterRuntime* runtime, ParticleSystem* system, ParticleRecord* record) RETAIL(GenCode4_VelocityXZ16xStart);
    void GenCode5_PullInRandomLife(EmitterRuntime* runtime, ParticleSystem* system, ParticleRecord* record) RETAIL(GenCode5_PullInRandomLife);
    void GenCode6_Velocity5_4xStart(EmitterRuntime* runtime, ParticleSystem* system, ParticleRecord* record) RETAIL(GenCode6_Velocity5_4xStart);
    // An emitter's blocks grown to the ones it wants (taken while there are free ones, the first listed for drawing) or shrunk (the
    // ones let go of drawing on their own until their particles are gone, then released); its blocks chained into one packet
    void AllocParticleEmitterBlocks(EmitterRuntime* runtime) RETAIL(AllocParticleEmitterBlocks);
    // The particles' start-up (a static initialiser's: the call to initialise with priority 0xFFFF): the decal pool, the particles'
    // graphics, and system 0 the "null" system
    void InitParticles(s32 initialise, s32 priority) RETAIL(FUN_001b95b8);
    // Every runtime of the game's (not the sections' emitters) let go of with its system's draws, the particle time moved 10 s on
    void KillParticles() RETAIL(FUN_001ba240);
    // Every section slot in use unloaded
    void UnloadAllParticleSections() RETAIL(FUN_001ba170);
    // No emitters (none of them made), no section slots in use
    void ResetParticleEmitters() RETAIL(FUN_001ba1d0);
    // A runtime's emission made to keep its translation: its place
    void KeepEmitterTranslation(s32 runtime) RETAIL(FUN_001ba0c8);
    // The section's reading given up: the reader copied into the copy stream when it wasn't set (rewound first), the streams and
    // the file let go
    void AbandonParticleSection() RETAIL(FUN_0019b170);
    // A loaded system by name: the slot's own first, then the default particles', then any (-1 none)
    s32 FindParticleSystem(const char* name, s8 slot) RETAIL(GetLoadedParticleIndexByName);
}
