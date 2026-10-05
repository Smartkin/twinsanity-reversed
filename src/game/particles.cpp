#include "game/particles.h"

#include "abi.h"

#include "game/chunkdata.h"
#include "game/chunkfiles.h"
#include "game/chunkloading.h"
#include "game/clock.h"
#include "game/controllers.h"
#include "game/decals.h"
#include "game/disk.h"
#include "game/memory.h"
#include "game/renderer.h"
#include "game/reference.h"
#include "game/stream.h"
#include "game/string.h"
#include "retail/libc.h"

#include "platform/graphics.h"
#include "platform/math.h"

namespace
{
// The versions the systems' and emitters' fields came in (older files get their defaults), and the legacy version, whose files
// have a layout of their own
enum ParticleVersion : s32
{
    VersionCollisionSpheres = 3,
    OldestVersion = 5,
    VersionCutRadiuses = 6,
    // Systems lose two words before their velocity, emitters get the gravity's turns (and their turns in halfwords)
    VersionWithoutOldWords = 7,
    VersionGravityTurns = 7,
    VersionTimingOffset = 8,
    VersionSwitches = 9,
    VersionDrawCutOff = 10,
    VersionBouncePlane = 0xC,
    VersionBounceFactor = 0xD,
    VersionGroup = 0xF,
    VersionGhosts = 0x10,
    VersionDrawList = 0x11,
    // The random emit and start once (they were there twice, the second dropped), the radial tilts halved until 0x14
    VersionSingleRandoms = 0x12,
    VersionHalvedTilts = 0x12,
    VersionWholeTilts = 0x14,
    VersionDistortion = 0x15,
    VersionEmitRoll = 0x16,
    // Records of 0x10 bytes and two words after the draw list, dropped (until 0x1D)
    VersionUnusedFloat5 = 0x17,
    VersionRecords = 0x17,
    VersionUnusedFloat6 = 0x18,
    VersionStar = 0x19,
    VersionRamp = 0x1A,
    VersionTexturePage = 0x1B,
    VersionScaleFactor = 0x1C,
    VersionWithoutRecords = 0x1D,
    VersionBoundingExtents = 0x1E,
    NewestVersion = 0x1E,
    LegacyVersion = 0x20,
};
// The blend modes past 3 a legacy file has are 4 more than they are now
constexpr s8 LegacyBlendShift = 4;
// The legacy files' records at the end of a system
constexpr s32 LegacyRecords = 4;
constexpr f32 DefaultCutOffRadius = 25.0f;
// A draw distance of 2 or less is none
constexpr f32 NoDrawCutOff = 2.0f;
constexpr f32 FarDrawCutOff = 0x1.E847FCp+19f;
constexpr f32 DefaultUnusedFloat6 = 0.5f;
constexpr f32 DefaultDistortion = 0.125f;
constexpr s16 DefaultStarPoints = 5;
constexpr f32 DefaultStarRadiusRatio = 0.5f;
// The culling box of the old files: the box generator's reach over the life, plus the biggest size (ten thousandths), three
// quarters of it; 10 for the others
constexpr f32 TenThousandth = 0x1.A36E2Ep-14f;
constexpr f32 ExtentShare = 0.75f;
constexpr f32 OtherExtent = 10.0f;
// A runtime's blocks hold 32 particles (12 hexagons)
constexpr s32 BlockParticles = 32;
constexpr s32 HexagonBlockParticles = 12;
constexpr s16 MaxBlockParticles = MaxEmitterBlocks * BlockParticles;
constexpr s16 MaxHexagonParticles = MaxEmitterBlocks * HexagonBlockParticles;
constexpr s16 NoSwitch = -1;
constexpr f32 DefaultBounceFactor = 0x1.CCCCCCp-1f;
// Two events a block
constexpr s32 EventsPerBlock = 2;
constexpr f32 PalFrame = 0x1.47AE14p-6f;
constexpr f32 NtscFrame = 0x1.111112p-6f;
// The texture rectangles' pixels get 2^19 more, so only their low 10 bits survive the GS's fixed point
constexpr f32 TexturePixelBias = 0x1.0p+19f;
// A block's release is due four frames on
constexpr u32 ReleaseDelay = 4;
constexpr f32 NoSphere = -1.0f;
// A new runtime's camera distance until it's run: far away
constexpr f32 NotRunDistance = 0x1.E847E0p+19f;
// The distance of a chunk not drawn this frame
constexpr f32 NoViewDistance = 0x1.312D00p+23f;
// What the reader's set-up returns (nothing reads it)
constexpr u32 ReaderSet = 0x65;

// The random spreads' and starts' axes for the radial generators: the radius (the emit's: the speed), the yaw and the tilt
enum RadialAxis : u32
{
    RadialRadius = 0,
    RadialSpeed = 0,
    RadialYaw = 1,
    RadialTilt = 2,
};

// The texture animation's frame rate as the set-up that never runs leaves it: the float with its low 11 bits made the frame count
// times the start frame less one (its low 10 bits)
union PackedFrameRate
{
    f32 rate;
    u32 value;
    struct
    {
        u32 startOffset : 10;
        u32 unused10 : 1;
        u32 rateBits : 21;
    };
};
CHECK_SIZE(PackedFrameRate, 4);

// A system of distorting hexagons
bool DrawsHexagons(const ParticleSystem* system)
{
    return static_cast<s8>(system->blendMode) == BlendDistortion;
}

ParticleDrawEntry* EndOfDrawEntries()
{
    return g_ParticleDrawEntries + (g_ParticleBlockCount + g_HexagonBlockCount);
}

// A block's pending events back in the pool
void DropBlockEvents(const u8* block)
{
    for (u32 frame = 0; frame < ParticleWheelFrames; frame++)
    {
        ParticleEvent* event = g_ParticleEventWheel[frame];
        if (event == nullptr)
        {
            continue;
        }

        ParticleEvent* next = event->next;
        while (true)
        {
            if (event->block == block)
            {
                ParticleEvent* head = g_ParticleEventWheel[frame];
                if (head != nullptr)
                {
                    if (head == event)
                    {
                        g_ParticleEventWheel[frame] = next;
                    }
                    else
                    {
                        for (ParticleEvent* before = head; before->next != nullptr; before = before->next)
                        {
                            if (before->next == event)
                            {
                                before->next = next;
                                break;
                            }
                        }
                    }
                }

                g_ParticleEventPoolTop--;
                event->next = nullptr;
                g_ParticleEventPool[g_ParticleEventPoolTop] = event;
                event->block = nullptr;
            }

            if (next == nullptr)
            {
                break;
            }

            event = next;
            next = next->next;
        }
    }
}

// The block's release, due four frames on
void QueueBlockRelease(u8* block, const ParticleSystem* system)
{
    ParticleEvent* event = g_ParticleEventPool[g_ParticleEventPoolTop];
    event->block = block;
    event->delay = 0;
    event->kind = DrawsHexagons(system) ? ParticleEvent::EndHexagonChain : ParticleEvent::EndParticleChain;
    event->runtime = nullptr;
    ParticleEvent** link = &g_ParticleEventWheel[(g_ParticleEventWheelIndex + ReleaseDelay) & (ParticleWheelFrames - 1)];
    while (*link != nullptr)
    {
        link = &(*link)->next;
    }

    *link = event;
    event->next = nullptr;
    g_ParticleEventPoolTop++;
}

// A dead particle's life scale: its life is over at once
constexpr f32 DeadParticle = 32768.0f;

// The block's particles made none
void ClearBlock(u8* block, s32 particles)
{
    auto* records = reinterpret_cast<ParticleRecord*>(block + ParticleBlockHeader);
    for (s32 record = 0; record < particles; record++)
    {
        records[record].spawnTime = 0.0f;
        records[record].lifeScale = DeadParticle;
    }
}

// The entry made to draw a block on its own with what the runtime had
void TakeOverBlock(ParticleDrawEntry* entry, const EmitterRuntime* runtime)
{
    entry->owner = nullptr;
    entry->system = g_LoadedParticleSystems[runtime->system];
    entry->matrix = runtime->gravityMatrix;
    entry->chunk = runtime->chunk;
    entry->position[0] = runtime->position[0];
    entry->position[1] = runtime->position[1];
    entry->position[2] = runtime->position[2];
    entry->texturePage = static_cast<s16>(g_LoadedParticleSystems[runtime->system]->texturePage);
    entry->keepsEmitTranslation = runtime->keepsEmitTranslation;
}

// The entry put first in its system's draw list
void ListDrawEntry(ParticleDrawEntry* entry, const ParticleSystem* system)
{
    ParticleDrawEntry*& list = g_ParticleDrawLists[static_cast<s8>(system->drawList)];
    if (list != nullptr)
    {
        list->previous = entry;
    }

    entry->next = list;
    list = entry;
}

// A stopped runtime is looked at again in 50 frames and up to 7 more, a released one past a turn of the wheel
constexpr s16 RestPhaseBase = 0x32;
constexpr s32 RestPhaseSpread = 8;
constexpr s16 ReleasedPhase = 0x21;

// The runtime put first in a wheel slot's list
void PushEmitter(EmitterRuntime* runtime, EmitterRuntime** slot)
{
    if (*slot != nullptr)
    {
        (*slot)->previous = runtime;
    }

    runtime->next = *slot;
    *slot = runtime;
}

// A free block's particles are born far in the future
constexpr f32 FreeSpawnTime = 0x1.2A05F2p+33f;
constexpr f32 FreeLifeScale = 128.0f;

// The event taken out of the current frame's list
void TakeOutOfFrame(ParticleEvent* event)
{
    ParticleEvent*& head = g_ParticleEventWheel[g_ParticleEventWheelIndex];
    if (head == nullptr)
    {
        return;
    }

    if (head == event)
    {
        head = event->next;
        return;
    }

    for (ParticleEvent* before = head; before->next != nullptr; before = before->next)
    {
        if (before->next == event)
        {
            before->next = event->next;
            break;
        }
    }
}

// The event (out of every list) due in a number of frames
void MoveToFrame(ParticleEvent* event, s32 frames)
{
    event->next = nullptr;
    AppendParticleEvent(event, &g_ParticleEventWheel[(g_ParticleEventWheelIndex + frames) & (ParticleWheelFrames - 1)]);
    event->delay = 0;
}

// The event (out of every list) back in the pool
void ReleaseEvent(ParticleEvent* event)
{
    g_ParticleEventPoolTop--;
    event->next = nullptr;
    g_ParticleEventPool[g_ParticleEventPoolTop] = event;
    event->block = nullptr;
}

// The block's particles made never to be born
void FreeBlock(u8* block, s32 particles)
{
    for (s32 record = 0; record < particles; record++)
    {
        auto* records = reinterpret_cast<ParticleRecord*>(block + ParticleBlockHeader);
        records[record].spawnTime = FreeSpawnTime;
        records[record].lifeScale = FreeLifeScale;
    }
}

// The runtime's blocks after one moved down over it (without the loop made into a call of memmove, which the game doesn't have)
__attribute__((optimize("no-tree-loop-distribute-patterns"))) void MoveBlocksDown(EmitterRuntime* runtime, s32 taken)
{
    for (s32 block = taken; block < runtime->blockCount - 1; block++)
    {
        runtime->blocks[block] = runtime->blocks[block + 1];
    }
}

// A curve's value at a share of the life: between the first pair of keys around it, 0 outside them
f32 SampleCurve(const ParticleKey* keys, f32 fraction)
{
    for (u32 key = 0; key < ParticleCurveKeys - 1; key++)
    {
        if (keys[key].time <= fraction && fraction <= keys[key + 1].time)
        {
            return keys[key].value + (fraction - keys[key].time) / (keys[key + 1].time - keys[key].time) * (keys[key + 1].value - keys[key].value);
        }
    }

    return 0.0f;
}

// A particle's life is the render table's 64 steps; a turn's 65536ths as a float
constexpr f32 LifeSteps = Platform::Graphics::ParticleRenderSteps;
constexpr f32 UnitsPerTurn = FullTurnAngle;

// The runtime's slot past its blocks' particles: the first again
void WrapSlot(EmitterRuntime* runtime)
{
    if (runtime->nextSlot >= runtime->capacity)
    {
        runtime->nextSlot = 0;
    }
}

// A block's particles: 12 hexagons for the distorting systems, 32 otherwise
s32 BlockSize(const ParticleSystem* system)
{
    return DrawsHexagons(system) ? HexagonBlockParticles : BlockParticles;
}

// The particle at a slot of the runtime's blocks of a size (the generators without hexagons go by 32 whatever the system's)
ParticleRecord* RecordAt(const EmitterRuntime* runtime, s32 slot, s32 perBlock)
{
    return reinterpret_cast<ParticleRecord*>(runtime->blocks[slot / perBlock] + ParticleBlockHeader) + slot % perBlock;
}

// The particle at the runtime's next slot, the slot moved on
ParticleRecord* TakeRecord(EmitterRuntime* runtime, s32 perBlock)
{
    ParticleRecord* record = RecordAt(runtime, runtime->nextSlot, perBlock);
    runtime->nextSlot = static_cast<s16>(runtime->nextSlot + 1);
    return record;
}

// Born at the end of the frame, living the system's life
void StartRecord(ParticleRecord* record, const ParticleSystem* system)
{
    record->spawnTime = g_ParticleTime + g_ParticleFrameDelta;
    record->lifeScale = LifeSteps / system->lifeTime;
}

// The ranges' particles are born at the frame's start
void StartRangeRecord(ParticleRecord* record, const ParticleSystem* system)
{
    record->spawnTime = g_ParticleTime;
    record->lifeScale = LifeSteps / system->lifeTime;
}

// A life up to 1/0.7 longer (of rand's 2^31)
f32 RandomLifeScale(const ParticleSystem* system)
{
    constexpr f32 LongerLife = 0x1.6DB6DCp-31f;
    f32 longer = system->lifeTime * static_cast<f32>(GetRand()) * LongerLife;
    return LifeSteps / (system->lifeTime + longer);
}

// A value of the C library's rand: up to 2^31 times a scale past a base
f32 RandomRange(f32 scale, f32 base)
{
    return static_cast<f32>(GetRand()) * scale + base;
}

// A radial generator's radius: grown from 0 over the ramp's time since the runtime was made
f32 RampedRadius(const EmitterRuntime* runtime, const ParticleSystem* system)
{
    f32 radius = system->randomStart[RadialRadius];
    if (system->rampTime != 0.0f)
    {
        f32 age = g_ParticleTime - runtime->creationTime;
        if (age < system->rampTime)
        {
            radius = radius * (age / system->rampTime);
        }
    }

    return radius;
}


// Up to a spread either way
f32 RandomSpread(f32 spread)
{
    f32 random = RandomFloat01(&g_ParticleRandom);
    return (random + random) * spread - spread;
}

// The point's coordinates into the record's
void StoreVector(f32* out, const Vector4& vector)
{
    out[0] = vector.x;
    out[1] = vector.y;
    out[2] = vector.z;
}

// A box's start and velocity, turned by the emission's matrix
void BoxStart(EmitterRuntime* runtime, const ParticleSystem* system, ParticleRecord* record)
{
    Vector4 point;
    point.x = RandomSpread(system->randomStart[0]);
    point.y = RandomSpread(system->randomStart[1]);
    point.z = RandomSpread(system->randomStart[2]);
    TransformPoint(&point, &point, &runtime->emitMatrix);
    StoreVector(record->start, point);
}

void BoxVelocity(EmitterRuntime* runtime, const ParticleSystem* system, ParticleRecord* record)
{
    Vector4 velocity;
    velocity.x = RandomSpread(system->randomEmit[0]);
    velocity.y = RandomSpread(system->randomEmit[1]) + system->velocity;
    velocity.z = RandomSpread(system->randomEmit[2]);
    TransformVector(&velocity, &velocity, &runtime->emitMatrix);
    StoreVector(record->velocity, velocity);
}

// The radial generators' start and velocity: out along the turned up from the middle
void RadialStart(EmitterRuntime* runtime, const ParticleSystem* system, ParticleRecord* record, f32 radius, s32 tilt, s32 yaw)
{
    Vector4 point = {0.0f, radius, 0.0f, 0.0f};
    RotateVectorZ(&point, &point, tilt);
    RotateVectorY(&point, &point, yaw);
    TransformPoint(&point, &point, &runtime->emitMatrix);
    StoreVector(record->start, point);
    Vector4 velocity = {0.0f, RandomSpread(system->randomEmit[RadialSpeed]) + system->velocity, 0.0f, 0.0f};
    RotateVectorZ(&velocity, &velocity, tilt);
    RotateVectorY(&velocity, &velocity, yaw);
    TransformVector(&velocity, &velocity, &runtime->emitMatrix);
    StoreVector(record->velocity, velocity);
}

// A turn's 65536ths through radians and back, as the sphere generators round them
s16 ThroughRadians(s32 angle)
{
    return static_cast<s16>(static_cast<s32>(static_cast<f32>(angle) * AngleToRadians * RadiansToAngle));
}

// The runtime's velocity rule, and its offsets
void ApplyVelocityRule(EmitterRuntime* runtime, ParticleSystem* system, ParticleRecord* record)
{
    if (runtime->velocityRule != nullptr)
    {
        runtime->velocityRule(runtime, system, record);
    }
}

void AddOffsets(const EmitterRuntime* runtime, ParticleRecord* record)
{
    record->start[0] = record->start[0] + runtime->startOffset[0];
    record->start[1] = record->start[1] + runtime->startOffset[1];
    record->start[2] = record->start[2] + runtime->startOffset[2];
    record->velocity[0] = record->velocity[0] + runtime->velocityOffset[0];
    record->velocity[1] = record->velocity[1] + runtime->velocityOffset[1];
    record->velocity[2] = record->velocity[2] + runtime->velocityOffset[2];
}

// The particle's ghosts in the next slots: copies of it born a separation after each other
void AddGhosts(EmitterRuntime* runtime, const ParticleSystem* system, const ParticleRecord* record)
{
    for (s32 ghost = 0; ghost < system->ghosts; ghost++)
    {
        WrapSlot(runtime);
        ParticleRecord* copy = TakeRecord(runtime, BlockSize(system));
        *copy = *record;
        copy->spawnTime = copy->spawnTime + static_cast<f32>(ghost + 1) * system->ghostSeparation;
    }
}

// The system's render table made when it has none
ParticleRecord* EndRecord(ParticleSystem* system, ParticleRecord* record)
{
    if (system->renderTable == nullptr)
    {
        BuildParticleRenderTable(system);
    }

    return record;
}

// When a particle at a height above a plane with an upward velocity is on it under a gravity (half the acceleration): the two
// times of the quadratic, the same twice when it touches the plane once or there's no gravity. Whether it ever is
bool PlaneTimes(f32 above, f32 velocity, f32 gravity, f32* first, f32* second)
{
    if (gravity == 0.0f)
    {
        if (velocity == 0.0f)
        {
            return false;
        }

        *first = -above / velocity;
        *second = *first;
        return true;
    }

    f32 square = velocity * velocity;
    f32 product = gravity * (above * 4.0f);
    if (square < product)
    {
        return false;
    }

    if (product == square)
    {
        *first = -velocity / (gravity + gravity);
        *second = *first;
        return true;
    }

    f32 root = __builtin_sqrtf(square - product);
    *first = (-velocity + root) / (gravity + gravity);
    *second = (-velocity - root) / (gravity + gravity);
    return true;
}

// A curve's value at a share of the life for the render table: the first pair of keys around it, the first key's value on its
// time, 0 outside them
f32 SampleRenderCurve(const ParticleKey* keys, f32 life)
{
    for (u32 key = 0; key < ParticleCurveKeys - 1; key++)
    {
        if (keys[key].time <= life && life <= keys[key + 1].time)
        {
            f32 into = life - keys[key].time;
            if (into == 0.0f)
            {
                return keys[key].value;
            }

            return keys[key].value + into / (keys[key + 1].time - keys[key].time) * (keys[key + 1].value - keys[key].value);
        }
    }

    return 0.0f;
}

// The colour curve the same way (black outside its keys)
void SampleRenderColour(const ParticleColourKey* keys, f32 life, f32* colour)
{
    for (u32 key = 0; key < ParticleCurveKeys - 1; key++)
    {
        const ParticleColourKey& from = keys[key];
        const ParticleColourKey& to = keys[key + 1];
        if (from.time <= life && life <= to.time)
        {
            f32 into = life - from.time;
            if (into == 0.0f)
            {
                colour[0] = from.red;
                colour[1] = from.green;
                colour[2] = from.blue;
                return;
            }

            f32 share = into / (to.time - from.time);
            colour[0] = from.red + share * (to.red - from.red);
            colour[1] = from.green + share * (to.green - from.green);
            colour[2] = from.blue + share * (to.blue - from.blue);
            return;
        }
    }

    colour[0] = 0.0f;
    colour[1] = 0.0f;
    colour[2] = 0.0f;
}

// A chunk's view for the frame's particles: none when it wasn't drawn this frame, loaded (and listed) the first time
s32 ParticleViewOf(ParticleViews* views, ChunkData* chunk)
{
    if (views->frame != chunk->drawnFrame)
    {
        return -1;
    }

    s32 view = chunk->particleView;
    if (view >= 0)
    {
        return view;
    }

    views->chunks[views->count] = chunk;
    view = Platform::Graphics::LoadParticleView(&chunk->matrix, views->count);
    chunk->particleView = view;
    views->count++;
    return view;
}

// The entry off its system's draw list
void UnlinkDrawEntry(ParticleDrawEntry* entry, const ParticleSystem* system)
{
    ParticleDrawEntry*& list = g_ParticleDrawLists[static_cast<s8>(system->drawList)];
    if (list == entry)
    {
        list = entry->next;
        if (entry->next != nullptr)
        {
            entry->next->previous = nullptr;
        }
    }
    else if (entry->previous != nullptr)
    {
        entry->previous->next = entry->next;
        if (entry->next != nullptr)
        {
            entry->next->previous = entry->previous;
        }
    }

    entry->previous = nullptr;
    entry->next = nullptr;
}
}

extern "C"
{
    void ReadParticleBytes(void* buffer, u32 size)
    {
        g_ParticleReader->Read(buffer, size, 1);
    }

    f32 ReadParticleFloat()
    {
        f32 value;
        g_ParticleReader->ReadF32(&value);
        return value;
    }

    u32 ReadParticleWord()
    {
        u32 value;
        g_ParticleReader->ReadU32(&value);
        return value;
    }

    s32 ReadParticleHalf()
    {
        u16 value;
        g_ParticleReader->ReadU16(&value);
        return static_cast<s16>(value);
    }

    s32 ReadParticleByte()
    {
        u8 value;
        g_ParticleReader->ReadU8(&value);
        return static_cast<s8>(value);
    }

    u32 SetParticleReader(Stream* stream)
    {
        g_ParticleReader = stream;
        g_ParticleReaderSet = 1;
        return ReaderSet;
    }

    void AbandonParticleSection()
    {
        constexpr s32 FileLetGo = -2;
        if (g_ParticleReaderSet == 0)
        {
            g_ParticleReader->Rewind();
            g_ParticleReader->CopyTo(g_ParticleCopyStream);
        }

        if (g_ParticleCopyStream != nullptr)
        {
            g_ParticleCopyStream->Destroy(DestroyAndFree);
        }

        if (g_ParticleOtherStream != nullptr)
        {
            g_ParticleOtherStream->Destroy(DestroyAndFree);
        }

        if (g_ParticleSectionFile >= 0)
        {
            DiskRelease(GetDiskManager(), &g_ParticleSectionFile);
            g_ParticleSectionFile = FileLetGo;
        }

        g_ParticleCopyStream = nullptr;
        g_ParticleOtherStream = nullptr;
        g_ParticleReader = nullptr;
    }

    s32 FindParticleSystem(const char* name, s8 slot)
    {
        for (s32 index = 1; index < static_cast<s32>(MaxParticleSystems); index++)
        {
            const ParticleSystem* system = g_LoadedParticleSystems[index];
            if (system != nullptr && static_cast<s8>(system->sectionSlot) == slot && RetailLibc::StringCompare(system->name, name) == 0)
            {
                return index;
            }
        }

        for (s32 index = 1; index < static_cast<s32>(MaxParticleSystems); index++)
        {
            const ParticleSystem* system = g_LoadedParticleSystems[index];
            if (system != nullptr && static_cast<s8>(system->sectionSlot) == DefaultParticleSlot && RetailLibc::StringCompare(system->name, name) == 0)
            {
                return index;
            }
        }

        for (s32 index = 1; index < static_cast<s32>(MaxParticleSystems); index++)
        {
            const ParticleSystem* system = g_LoadedParticleSystems[index];
            if (system != nullptr && RetailLibc::StringCompare(system->name, name) == 0)
            {
                return index;
            }
        }

        return -1;
    }

    void ComputeParticleBoundingExtents(ParticleSystem* system)
    {
        if (system->genSort == 0)
        {
            f32 size = system->maxSize * TenThousandth;
            for (u32 axis = 0; axis < 3; axis++)
            {
                f32 reach = (system->velocity + system->randomEmit[axis]) * system->lifeTime + system->randomStart[axis];
                system->boundingExtents[axis] = (reach + size) * ExtentShare;
            }
        }
        else
        {
            system->boundingExtents[0] = OtherExtent;
            system->boundingExtents[1] = OtherExtent;
            system->boundingExtents[2] = OtherExtent;
        }

        system->boundingExtents[3] = 0.0f;
    }

    // Every version's layout: what a version doesn't have gets its default, the legacy version (0x20) has its texture page
    // after the name, blend modes 4 more and records of its own at the end
    void ReadParticleSystem(ParticleSystem* system, s32 version, s32 kind)
    {
        u8 skipped[0x10];
        ReadParticleBytes(system->name, sizeof(system->name));
        if (version == LegacyVersion)
        {
            ReadParticleByte();
            system->texturePage = ReadParticleByte();
        }

        system->unused10 = static_cast<u8>(kind);
        system->genRate = static_cast<s16>(ReadParticleHalf());
        system->maxParticles = static_cast<u16>(ReadParticleHalf());
        system->timingOffset = static_cast<u16>(ReadParticleHalf());
        system->onTime = static_cast<u16>(ReadParticleHalf());
        system->onTimeRandom = static_cast<u16>(ReadParticleHalf());
        system->offTime = static_cast<u16>(ReadParticleHalf());
        system->offTimeRandom = static_cast<u16>(ReadParticleHalf());
        system->genSort = static_cast<u8>(ReadParticleByte());
        system->genCode = static_cast<u8>(ReadParticleByte());
        system->blendMode = static_cast<u8>(ReadParticleByte());
        system->unusedByte = static_cast<u8>(ReadParticleByte());
        system->unusedFloat1 = ReadParticleFloat();
        if (version == LegacyVersion)
        {
            system->unusedFloat1 = DefaultCutOffRadius;
            if (static_cast<s8>(system->blendMode) >= LegacyBlendShift)
            {
                system->blendMode = static_cast<u8>(system->blendMode - LegacyBlendShift);
            }
        }

        if (version < VersionCutRadiuses)
        {
            system->cutOnRadius = 0.0f;
            system->cutOffRadius = DefaultCutOffRadius;
        }
        else
        {
            system->cutOnRadius = ReadParticleFloat();
            system->cutOffRadius = ReadParticleFloat();
        }

        if (version < VersionDrawCutOff)
        {
            system->drawCutOff = 0.0f;
        }
        else
        {
            system->drawCutOff = ReadParticleFloat();
            if (system->drawCutOff <= NoDrawCutOff)
            {
                system->drawCutOff = FarDrawCutOff;
            }
        }

        system->unusedFloat5 = version < VersionUnusedFloat5 || version == LegacyVersion ? 0.0f : ReadParticleFloat();
        system->unusedFloat6 = version < VersionUnusedFloat6 || version == LegacyVersion ? DefaultUnusedFloat6 : ReadParticleFloat();
        if (version < VersionWithoutOldWords)
        {
            ReadParticleWord();
            ReadParticleWord();
        }

        system->velocity = ReadParticleFloat();
        ReadParticleBytes(system->randomEmit, sizeof(system->randomEmit));
        if (version < VersionSingleRandoms)
        {
            ReadParticleBytes(skipped, sizeof(system->randomEmit));
        }

        ReadParticleBytes(system->randomStart, sizeof(system->randomStart));
        if (version < VersionSingleRandoms)
        {
            ReadParticleBytes(skipped, sizeof(system->randomStart));
        }

        // Versions 0x12 and 0x13 kept the radial generator's tilts halved
        if (static_cast<u32>(version - VersionHalvedTilts) < VersionWholeTilts - VersionHalvedTilts &&
            static_cast<s8>(system->genSort) == GenSortRadial)
        {
            system->randomEmit[RadialTilt] = system->randomEmit[RadialTilt] + system->randomEmit[RadialTilt];
            system->randomStart[RadialTilt] = system->randomStart[RadialTilt] + system->randomStart[RadialTilt];
        }

        for (f32& value : system->startRandomScale)
        {
            value = ReadParticleFloat();
        }

        for (f32& value : system->startBase)
        {
            value = ReadParticleFloat();
        }

        for (f32& value : system->velocityRandomScale)
        {
            value = ReadParticleFloat();
        }

        for (f32& value : system->velocityBase)
        {
            value = ReadParticleFloat();
        }

        system->gravity = ReadParticleFloat();
        system->lifeTime = ReadParticleFloat();
        system->textureFrameCount = static_cast<u16>(ReadParticleHalf());
        system->textureFrameStart = static_cast<u8>(ReadParticleByte());
        system->textureFrameHold = static_cast<u8>(ReadParticleByte());
        system->textureFrameRate = ReadParticleFloat();
        system->jibberXFreq = ReadParticleFloat();
        system->jibberXAmp = ReadParticleFloat();
        system->jibberYFreq = ReadParticleFloat();
        system->jibberYAmp = ReadParticleFloat();
        for (ParticleColourKey& key : system->colourKeys)
        {
            ReadParticleBytes(&key, sizeof(key));
        }

        for (ParticleKey& key : system->alphaKeys)
        {
            ReadParticleBytes(&key, sizeof(key));
        }

        if (version < VersionDistortion)
        {
            system->distortionY = DefaultDistortion;
            system->distortionX = DefaultDistortion;
        }
        else
        {
            system->distortionX = ReadParticleFloat();
            system->distortionY = ReadParticleFloat();
        }

        system->minSize = ReadParticleFloat();
        system->maxSize = ReadParticleFloat();
        for (ParticleKey& key : system->widthKeys)
        {
            ReadParticleBytes(&key, sizeof(key));
        }

        for (ParticleKey& key : system->heightKeys)
        {
            ReadParticleBytes(&key, sizeof(key));
        }

        system->minRotation = ReadParticleFloat();
        system->maxRotation = ReadParticleFloat();
        for (ParticleKey& key : system->rotationKeys)
        {
            ReadParticleBytes(&key, sizeof(key));
        }

        for (ParticleKey& key : system->unusedKeys1)
        {
            ReadParticleBytes(&key, sizeof(key));
        }

        for (ParticleKey& key : system->unusedKeys2)
        {
            ReadParticleBytes(&key, sizeof(key));
        }

        system->textureStartX = ReadParticleFloat();
        system->textureStartY = ReadParticleFloat();
        system->textureEndX = ReadParticleFloat();
        system->textureEndY = ReadParticleFloat();
        if (version == LegacyVersion)
        {
            ReadParticleWord();
        }

        if (version < VersionCollisionSpheres)
        {
            system->collisionSpheres = 0;
        }
        else
        {
            for (ParticleKey& key : system->collisionRadiusKeys)
            {
                ReadParticleBytes(&key, sizeof(key));
            }

            system->collisionSpheres = static_cast<u8>(ReadParticleByte());
        }

        system->drawList = version < VersionDrawList ? DrawListParticles : static_cast<u8>(ReadParticleByte());
        if (DrawsHexagons(system))
        {
            system->drawList = DrawListDistortion;
        }

        for (s32 slot = SystemEmitterSlots - 1; slot >= 0; slot--)
        {
            system->emitterSlots[slot] = -1;
        }

        if (version == LegacyVersion)
        {
            ReadParticleByte();
            ReadParticleByte();
            ReadParticleWord();
        }

        if (version >= VersionRecords && version != LegacyVersion)
        {
            // Records of 0x10 bytes and two words, dropped
            if (version < VersionWithoutRecords)
            {
                s32 count = static_cast<s32>(ReadParticleWord());
                for (s32 left = count; left > 0; left--)
                {
                    ReadParticleBytes(skipped, sizeof(skipped));
                    ReadParticleWord();
                    ReadParticleWord();
                }
            }
        }
        else if (version == LegacyVersion)
        {
            // Its records: the first's float is the scale
            for (s32 record = 0; record < LegacyRecords; record++)
            {
                if (record == 0)
                {
                    system->scaleFactor = ReadParticleFloat();
                    ReadParticleWord();
                    ReadParticleWord();
                }
                else
                {
                    ReadParticleWord();
                    ReadParticleWord();
                    ReadParticleWord();
                }
            }
        }

        if (version < VersionGhosts)
        {
            system->ghosts = 0;
            system->ghostSeparation = 0.0f;
        }
        else
        {
            if (version == LegacyVersion)
            {
                system->ghosts = static_cast<s16>(ReadParticleHalf());
                ReadParticleByte();
                ReadParticleByte();
            }
            else
            {
                system->ghosts = static_cast<s16>(ReadParticleWord());
            }

            system->ghostSeparation = ReadParticleFloat();
        }

        if (version >= VersionStar && version != LegacyVersion)
        {
            system->starPoints = static_cast<s16>(ReadParticleWord());
            system->starRadiusRatio = ReadParticleFloat();
        }
        else
        {
            system->starPoints = DefaultStarPoints;
            system->starRadiusRatio = DefaultStarRadiusRatio;
        }

        system->rampTime = version < VersionRamp || version == LegacyVersion ? 0.0f : ReadParticleFloat();
        if (version != LegacyVersion)
        {
            if (version >= VersionTexturePage)
            {
                system->texturePage = static_cast<s32>(ReadParticleWord());
            }

            if (version >= VersionScaleFactor)
            {
                system->scaleFactor = ReadParticleFloat();
            }
        }

        if (version < VersionBoundingExtents)
        {
            ComputeParticleBoundingExtents(system);
        }
        else
        {
            ReadParticleBytes(system->boundingExtents, sizeof(system->boundingExtents));
        }
    }

    // The particles one lifetime of the on/off cycle makes, at least 1, times the ghosts and the particle itself; the emitters
    // running the system get their blocks for it
    void ComputeParticleMaxCount(ParticleSystem* system)
    {
        s32 lifeFrames = static_cast<s32>(system->lifeTime * FramesPerSecond);
        s32 count = 0;
        s32 frame = 0;
        s32 offLeft = 1;
        s32 onLeft = lifeFrames;
        if (static_cast<s16>(system->offTime) != 0)
        {
            onLeft = static_cast<s16>(system->onTime) + static_cast<s16>(system->onTimeRandom);
        }

        for (s32 left = lifeFrames; left > 0; left--)
        {
            if (onLeft == 0)
            {
                offLeft--;
                if (offLeft == 0)
                {
                    onLeft = static_cast<s16>(system->onTime) + static_cast<s16>(system->onTimeRandom);
                    frame = 0;
                }

                continue;
            }

            s32 rate = system->genRate;
            if (rate < 0)
            {
                if (frame % -rate == 0)
                {
                    count++;
                }
            }
            else
            {
                count += rate;
            }

            onLeft--;
            frame++;
            if (onLeft == 0)
            {
                offLeft = static_cast<s16>(system->offTime);
            }
        }

        system->maxParticles = static_cast<u16>(count);
        if (static_cast<s16>(count) <= 0)
        {
            system->maxParticles = 1;
        }

        system->maxParticles = static_cast<u16>(static_cast<s16>(system->maxParticles) * (system->ghosts + 1));
        for (const ParticleEmitter& emitter : g_ParticleEmitters)
        {
            if (emitter.runtime == -1 || emitter.runtime == EmitterNotMade)
            {
                continue;
            }

            EmitterRuntime& runtime = g_EmitterRuntimes[emitter.runtime];
            if (g_LoadedParticleSystems[runtime.system] != system)
            {
                continue;
            }

            s32 particles = static_cast<s16>(system->maxParticles);
            runtime.maxParticles = static_cast<s16>(particles);
            bool hexagons = DrawsHexagons(system);
            if (hexagons)
            {
                runtime.blocksWanted = static_cast<s16>((particles + HexagonBlockParticles - 1) / HexagonBlockParticles);
            }
            else
            {
                runtime.blocksWanted = static_cast<s16>((particles + BlockParticles - 1) / BlockParticles);
            }

            if (runtime.blocksWanted > MaxEmitterBlocks)
            {
                runtime.blocksWanted = MaxEmitterBlocks;
                runtime.maxParticles = hexagons ? MaxHexagonParticles : MaxBlockParticles;
            }

            if (runtime.blocksWanted == runtime.blockCount)
            {
                runtime.capacity = runtime.maxParticles;
            }
            else
            {
                AllocParticleEmitterBlocks(&runtime);
            }
        }
    }

    s32 ReadParticleSection(s8 kind, ChunkData* chunk)
    {
        s32 slot = -1;
        if (kind == SectionLevel)
        {
            if (g_ParticleSlotsUsed[FirstLevelParticleSlot] == 0)
            {
                slot = FirstLevelParticleSlot;
            }
            else
            {
                for (s32 candidate = FirstLevelParticleSlot + 1; candidate < static_cast<s32>(ParticleSectionSlots); candidate++)
                {
                    if (g_ParticleSlotsUsed[candidate] == 0)
                    {
                        slot = candidate;
                        break;
                    }
                }
            }
        }
        else
        {
            slot = 0;
        }

        if (slot == -1)
        {
            return -1;
        }

        s32 version = static_cast<s32>(ReadParticleWord());
        if ((version < OldestVersion || version > NewestVersion) && version != LegacyVersion)
        {
            AbandonParticleSection();
            return -1;
        }

        // Systems past the 300 are read and dropped
        s32 count = static_cast<s32>(ReadParticleWord());
        s32 dropped = 0;
        if (g_LoadedParticleSystemCount + count > static_cast<s32>(MaxParticleSystems))
        {
            dropped = g_LoadedParticleSystemCount + count - static_cast<s32>(MaxParticleSystems);
            count = static_cast<s32>(MaxParticleSystems) - g_LoadedParticleSystemCount;
        }

        s32 index = 1;
        for (s32 read = 0; read < count; read++)
        {
            while (g_LoadedParticleSystems[index] != nullptr)
            {
                index++;
            }

            ParticleSystem* system = &g_ParticleSystems[index];
            ReadParticleSystem(system, version, kind);
            system->sectionSlot = static_cast<u8>(slot);
            g_LoadedParticleSystems[index] = system;
            ComputeParticleMaxCount(system);
            g_LoadedParticleSystemCount++;
        }

        ParticleSystem droppedSystem;
        for (s32 left = dropped; left > 0; left--)
        {
            ReadParticleSystem(&droppedSystem, version, kind);
        }

        // Kinds 1 and 2 have emitters
        if (static_cast<u8>(kind - SectionLevel) < 2)
        {
            s32 emitters = static_cast<s32>(ReadParticleWord());
            if (version != LegacyVersion)
            {
                g_UnfoundParticleSystems = 0;
                if (g_ParticleEmitterCount + emitters > static_cast<s32>(MaxParticleEmitters))
                {
                    emitters = static_cast<s32>(MaxParticleEmitters) - g_ParticleEmitterCount;
                }

                s32 place = 0;
                for (s32 read = 0; read < emitters; read++)
                {
                    while (g_ParticleEmitters[place].runtime != -1)
                    {
                        place++;
                    }

                    ParticleEmitter& emitter = g_ParticleEmitters[place];
                    emitter.sectionSlot = static_cast<u8>(slot);
                    ReadParticleBytes(emitter.position, sizeof(emitter.position));
                    if (version < VersionGravityTurns)
                    {
                        emitter.gravityTilt = 0;
                        emitter.gravityYaw = 0;
                        emitter.emitTilt = static_cast<s16>(ReadParticleWord());
                        emitter.emitYaw = static_cast<s16>(ReadParticleWord());
                    }
                    else
                    {
                        emitter.gravityTilt = static_cast<s16>(ReadParticleHalf());
                        emitter.gravityYaw = static_cast<s16>(ReadParticleHalf());
                        emitter.emitTilt = static_cast<s16>(ReadParticleHalf());
                        emitter.emitYaw = static_cast<s16>(ReadParticleHalf());
                    }

                    emitter.emitRoll = version < VersionEmitRoll ? 0 : static_cast<s16>(ReadParticleHalf());
                    emitter.timingOffset = version < VersionTimingOffset ? 0 : static_cast<s32>(ReadParticleWord());
                    ReadParticleBytes(emitter.name, sizeof(emitter.name));
                    emitter.runtime = EmitterNotMade;
                    emitter.system = FindParticleSystem(emitter.name, static_cast<s8>(slot));
                    if (emitter.system == -1)
                    {
                        g_UnfoundParticleSystems++;
                    }

                    if (version < VersionSwitches)
                    {
                        emitter.switchType = 0;
                        emitter.switchId = NoSwitch;
                        emitter.switchValue = 0.0f;
                    }
                    else
                    {
                        emitter.switchType = static_cast<s32>(ReadParticleWord());
                        emitter.switchId = static_cast<s32>(ReadParticleWord());
                        emitter.switchValue = ReadParticleFloat();
                    }

                    if (version < VersionBouncePlane)
                    {
                        emitter.unusedShort = 0;
                        emitter.bouncePlaneAngle = 0;
                        emitter.planeOffset = 0.0f;
                    }
                    else
                    {
                        emitter.unusedShort = static_cast<s16>(ReadParticleHalf());
                        emitter.bouncePlaneAngle = static_cast<s16>(ReadParticleHalf());
                        emitter.planeOffset = ReadParticleFloat();
                    }

                    emitter.bounceFactor = version < VersionBounceFactor ? DefaultBounceFactor : ReadParticleFloat();
                    emitter.groupId = version < VersionGroup ? 0 : static_cast<s16>(ReadParticleHalf());
                    g_ParticleEmitterCount++;
                }
            }
        }

        g_ParticleSlotChunks[slot] = chunk;
        g_ParticleSlotsUsed[slot] = 1;
        if (chunk != nullptr)
        {
            chunk->particles = slot;
            CreateLoadedParticleEmitters(static_cast<s8>(slot));
        }

        return slot;
    }

    void CreateLoadedParticleEmitters(s8 slot)
    {
        ChunkData* chunk = g_ParticleSlotsUsed[slot] != 0 ? g_ParticleSlotChunks[slot] : nullptr;
        if (chunk == nullptr)
        {
            return;
        }

        for (ParticleEmitter& emitter : g_ParticleEmitters)
        {
            if (static_cast<s8>(emitter.sectionSlot) != slot || emitter.runtime != EmitterNotMade)
            {
                continue;
            }

            emitter.runtime = -1;
            emitter.system = FindParticleSystem(emitter.name, slot);
            CreateParticleEmitter(emitter.position[0], emitter.position[1], emitter.position[2], &emitter.runtime, emitter.system,
                                  chunk);
            if (emitter.runtime == -1)
            {
                emitter.runtime = EmitterNotMade;
                continue;
            }

            EmitterRuntime& runtime = g_EmitterRuntimes[emitter.runtime];
            runtime.gravityMatrix = g_IdentityMatrix;
            MatrixRotateZ(&runtime.gravityMatrix, emitter.gravityTilt);
            MatrixRotateY(&runtime.gravityMatrix, emitter.gravityYaw);
            runtime.emitMatrix = g_IdentityMatrix;
            MatrixRotateZ(&runtime.emitMatrix, emitter.emitTilt);
            MatrixRotateY(&runtime.emitMatrix, emitter.emitYaw);
            MatrixRotateX(&runtime.emitMatrix, emitter.emitRoll);
            SetParticleEmitterTiming(emitter.runtime, emitter.timingOffset);
            runtime.bounceFactor = emitter.bounceFactor;
            runtime.unusedShort = emitter.unusedShort;
            runtime.bouncePlaneAngle = emitter.bouncePlaneAngle;
            runtime.planeOffset = emitter.planeOffset;
            ChunkData* emitterChunk = g_ParticleSlotChunks[static_cast<s8>(emitter.sectionSlot)];
            runtime.state = EmitterRuntime::StateOfChunk;
            runtime.chunk = emitterChunk != nullptr ? emitterChunk : g_DrawnChunk;
        }

        g_ParticleSlotsMade[slot] = 1;
    }

    void UnloadParticleSection(s8 slot)
    {
        if (slot == -1)
        {
            return;
        }

        for (ParticleEmitter& emitter : g_ParticleEmitters)
        {
            if (static_cast<s8>(emitter.sectionSlot) != slot || emitter.runtime == -1)
            {
                continue;
            }

            if (emitter.runtime != EmitterNotMade)
            {
                ReleaseParticleEmitter(&emitter.runtime);
            }

            emitter.runtime = -1;
            g_ParticleEmitterCount--;
        }

        for (s32 index = 1; index < static_cast<s32>(MaxParticleSystems); index++)
        {
            ParticleSystem* system = g_LoadedParticleSystems[index];
            if (system == nullptr || static_cast<s8>(system->sectionSlot) != slot)
            {
                continue;
            }

            for (s32 runtime = 0; runtime < static_cast<s32>(MaxEmitterRuntimes); runtime++)
            {
                if (g_EmitterRuntimes[runtime].system == index)
                {
                    s32 released = runtime;
                    ReleaseParticleEmitter(&released);
                }
            }

            ForgetParticleSystemDraws(g_LoadedParticleSystems[index]);
            system = g_LoadedParticleSystems[index];
            if (system->renderTable != nullptr)
            {
                g_RenderTablePoolTop--;
                g_RenderTablePool[g_RenderTablePoolTop] = system->renderTable;
                system->renderTable = nullptr;
            }

            g_LoadedParticleSystems[index] = nullptr;
            g_LoadedParticleSystemCount--;
        }

        g_ParticleSlotsUsed[slot] = 0;
        g_ParticleSlotsMade[slot] = 0;
        g_ParticleSlotChunks[slot] = nullptr;
    }

    void InitParticleSystemsForLevel(s32 blocks)
    {
        g_ParticlesSetUp = 1;
        g_ParticleBlockCount = blocks;
        g_ParticleEvents =
            static_cast<ParticleEvent*>(MemoryAllocate2((g_HexagonBlockCount + blocks) * EventsPerBlock * sizeof(ParticleEvent)));
        g_ParticleEventPool = static_cast<ParticleEvent**>(
            MemoryAllocate2((g_ParticleBlockCount + g_HexagonBlockCount) * EventsPerBlock * sizeof(ParticleEvent*)));
        g_ParticleBlocks = static_cast<u8**>(MemoryAllocate2(g_ParticleBlockCount * sizeof(u8*)));
        g_HexagonBlocks = static_cast<u8**>(MemoryAllocate2(g_HexagonBlockCount * sizeof(u8*)));
        g_ParticleDrawEntries = static_cast<ParticleDrawEntry*>(
            MemoryAllocate2((g_ParticleBlockCount + g_HexagonBlockCount) * sizeof(ParticleDrawEntry)));
        RetailLibc::MemorySet(g_FreeEmitterRuntimes, 0, sizeof(g_FreeEmitterRuntimes));
        RetailLibc::MemorySet(g_EmitterRuntimes, 0, sizeof(g_EmitterRuntimes));
        g_ParticleFrameDelta = g_Pal ? PalFrame : NtscFrame;
        // From the table's first entry, which is never filled: this never runs
        if (g_LoadedParticleSystems[0] != nullptr)
        {
            for (ParticleSystem** entry = g_LoadedParticleSystems; *entry != nullptr; entry++)
            {
                ParticleSystem* system = *entry;
                if (static_cast<s8>(system->textureFrameHold) == 0)
                {
                    system->textureFrameHold = 1;
                }

                system->textureFrameRate = static_cast<f32>(static_cast<s16>(system->textureFrameCount)) * FramesPerSecond /
                                           static_cast<f32>(static_cast<s8>(system->textureFrameHold));
                auto& packed = *reinterpret_cast<PackedFrameRate*>(&system->textureFrameRate);
                packed.unused10 = 0;
                packed.startOffset = static_cast<s16>(system->textureFrameCount) * (static_cast<s8>(system->textureFrameStart) - 1);
                system->alphaKeys[0].value /= system->lifeTime;
                system->textureStartX += TexturePixelBias;
                system->textureEndX += TexturePixelBias;
                system->textureStartY += TexturePixelBias;
                system->textureEndY += TexturePixelBias;
            }
        }

        for (ParticleSystem& system : g_ParticleSystems)
        {
            system.renderTable = nullptr;
        }

        g_RenderTablePoolTop = 0;
        InitParticleBlocks(blocks);
        for (s32 runtime = MaxEmitterRuntimes - 1; runtime >= 0; runtime--)
        {
            g_FreeEmitterRuntimes[runtime] = static_cast<s16>(runtime);
        }

        for (EmitterRuntime& runtime : g_EmitterRuntimes)
        {
            runtime.enabled = 0;
            runtime.nextSlot = 0;
            runtime.rotorYaw = 0;
            runtime.rotorTilt = 0;
            runtime.cyclesLeft = 0;
            runtime.system = 0;
        }

        for (EmitterRuntime*& slot : g_ParticleEmitterWheel)
        {
            slot = nullptr;
        }

        g_ParticleEmitterWheelIndex = 0;
        Platform::Graphics::MakeDistortionMaterial(&g_DistortionMaterial);
    }

    s16 AllocParticleEmitterSlot()
    {
        if (g_TakenEmitterRuntimes >= static_cast<s32>(MaxEmitterRuntimes))
        {
            return -1;
        }

        s16 index = g_FreeEmitterRuntimes[g_TakenEmitterRuntimes];
        g_TakenEmitterRuntimes++;
        EmitterRuntime& runtime = g_EmitterRuntimes[index];
        for (s32 block = MaxEmitterBlocks - 1; block >= 0; block--)
        {
            runtime.blocks[block] = nullptr;
        }

        runtime.listsDraws = 1;
        runtime.state = EmitterRuntime::StateLoose;
        runtime.blockCount = 0;
        runtime.nextSlot = 0;
        runtime.capacity = 0;
        runtime.blocksWanted = 0;
        runtime.maxParticles = 0;
        runtime.unused108 = 0;
        runtime.chunk = nullptr;
        runtime.cameraSwitch = EmitterRuntime::CameraByDistance;
        return index;
    }

    // Random on or off times leave the phase 0; the rotor's angles start at the offset times its turns
    void SetParticleEmitterTiming(s32 runtimeIndex, s32 timingOffset)
    {
        if (runtimeIndex == -1)
        {
            return;
        }

        EmitterRuntime& runtime = g_EmitterRuntimes[runtimeIndex];
        const ParticleSystem* system = g_LoadedParticleSystems[runtime.system];
        if (system->onTimeRandom != 0 || system->offTimeRandom != 0)
        {
            runtime.phase = 0;
            return;
        }

        if (static_cast<s8>(system->genSort) == GenSortRadialRotor)
        {
            runtime.phase = 0;
            runtime.rotorYaw = static_cast<s16>(static_cast<s32>(static_cast<f32>(timingOffset) * system->randomEmit[RadialYaw]));
            runtime.rotorTilt = static_cast<s16>(static_cast<s32>(static_cast<f32>(timingOffset) * system->randomEmit[RadialTilt]));
        }

        s32 period = static_cast<s16>(system->onTime) + static_cast<s16>(system->offTime);
        s32 late = g_ParticleFrameCounter % period - timingOffset;
        runtime.phase = static_cast<s16>(late == 0 ? 0 : period - late);
        runtime.onTimeLeft = static_cast<s16>(system->onTime);
    }

    void DropEmitterBlocks(s32* runtimeIndex)
    {
        EmitterRuntime* runtime = &g_EmitterRuntimes[*runtimeIndex];
        const ParticleSystem* system = g_LoadedParticleSystems[runtime->system];
        if (runtime->blockCount == 0)
        {
            return;
        }

        if (runtime->blockCount > 0)
        {
            s32 block = 0;
            do
            {
                DropBlockEvents(g_EmitterRuntimes[*runtimeIndex].blocks[block]);
                QueueBlockRelease(g_EmitterRuntimes[*runtimeIndex].blocks[block], system);
                block++;
            } while (block < g_EmitterRuntimes[*runtimeIndex].blockCount);
        }

        for (ParticleDrawEntry* entry = g_ParticleDrawEntries; entry != EndOfDrawEntries(); entry++)
        {
            runtime = &g_EmitterRuntimes[*runtimeIndex];
            if (entry->block != runtime->blocks[0])
            {
                continue;
            }

            if (static_cast<s8>(runtime->listsDraws) == 0)
            {
                entry->block = nullptr;
            }
            else
            {
                UnlinkDrawEntry(entry, system);
                entry->block = nullptr;
            }

            entry->system = nullptr;
        }

        runtime = &g_EmitterRuntimes[*runtimeIndex];
        runtime->blockCount = 0;
        runtime->capacity = 0;
    }

    void ForgetParticleSystemDraws(ParticleSystem* system)
    {
        for (s32 index = 0; index < g_ParticleBlockCount + g_HexagonBlockCount; index++)
        {
            ParticleDrawEntry* entry = &g_ParticleDrawEntries[index];
            if (entry->owner != nullptr || entry->system != system)
            {
                continue;
            }

            DropBlockEvents(entry->block);
            QueueBlockRelease(g_ParticleDrawEntries[index].block, system);
            entry = &g_ParticleDrawEntries[index];
            UnlinkDrawEntry(entry, system);
            entry = &g_ParticleDrawEntries[index];
            entry->system = nullptr;
            entry->block = nullptr;
        }
    }

    void ReleaseParticleEmitter(s32* runtimeIndex)
    {
        if (*runtimeIndex == -1)
        {
            return;
        }

        // Already free: only the handle goes
        for (s32 freeSlot = g_TakenEmitterRuntimes; freeSlot < static_cast<s32>(MaxEmitterRuntimes); freeSlot++)
        {
            if (g_FreeEmitterRuntimes[freeSlot] == *runtimeIndex)
            {
                *runtimeIndex = -1;
                return;
            }
        }

        ParticleSystem* system = g_LoadedParticleSystems[g_EmitterRuntimes[*runtimeIndex].system];
        DropEmitterBlocks(runtimeIndex);
        EmitterRuntime** slot = FindEmitterInWheel(&g_EmitterRuntimes[*runtimeIndex]);
        if (slot != nullptr)
        {
            UnlinkEmitter(&g_EmitterRuntimes[*runtimeIndex], slot);
        }

        for (s16& emitterSlot : system->emitterSlots)
        {
            if (emitterSlot == *runtimeIndex)
            {
                emitterSlot = -1;
            }
        }

        g_TakenEmitterRuntimes--;
        g_EmitterRuntimes[*runtimeIndex].system = 0;
        g_EmitterRuntimes[*runtimeIndex].state = EmitterRuntime::StateLoose;
        g_FreeEmitterRuntimes[g_TakenEmitterRuntimes] = static_cast<s16>(*runtimeIndex);
        *runtimeIndex = -1;
    }

    // The slot whose list has it (the slot's place in the wheel, not the link)
    EmitterRuntime** FindEmitterInWheel(EmitterRuntime* runtime)
    {
        for (EmitterRuntime*& slot : g_ParticleEmitterWheel)
        {
            for (EmitterRuntime* listed = slot; listed != nullptr; listed = listed->next)
            {
                if (listed == runtime)
                {
                    return &slot;
                }
            }
        }

        return nullptr;
    }

    void UnlinkEmitter(EmitterRuntime* runtime, EmitterRuntime** slot)
    {
        if (runtime->previous == nullptr)
        {
            *slot = runtime->next;
            if (runtime->next != nullptr)
            {
                runtime->next->previous = nullptr;
            }
        }
        else
        {
            runtime->previous->next = runtime->next;
            if (runtime->next != nullptr)
            {
                runtime->next->previous = runtime->previous;
            }
        }

        runtime->next = nullptr;
        runtime->previous = nullptr;
    }

    void SetEmitterChunk(s32 runtimeIndex, ChunkData* chunk)
    {
        EmitterRuntime& runtime = g_EmitterRuntimes[runtimeIndex];
        runtime.chunk = chunk != nullptr ? chunk : g_DrawnChunk;
        runtime.state = EmitterRuntime::StateOfChunk;
    }

    void SetEmitterGravity(s32 runtimeIndex, const Matrix4x4* matrix)
    {
        if (runtimeIndex == -1)
        {
            return;
        }

        EmitterRuntime& runtime = g_EmitterRuntimes[runtimeIndex];
        runtime.gravityMatrix = *matrix;
        runtime.position[0] = matrix->m[3][0];
        runtime.position[1] = matrix->m[3][1];
        runtime.position[2] = matrix->m[3][2];
        runtime.gravityMatrix.m[3][2] = 0.0f;
        runtime.gravityMatrix.m[3][0] = 0.0f;
        runtime.gravityMatrix.m[3][1] = 0.0f;
    }

    void SetEmitterEmission(s32 runtimeIndex, const Matrix4x4* matrix)
    {
        if (runtimeIndex == -1)
        {
            return;
        }

        EmitterRuntime& runtime = g_EmitterRuntimes[runtimeIndex];
        runtime.emitMatrix = *matrix;
        if (static_cast<s8>(runtime.keepsEmitTranslation) == 0)
        {
            runtime.emitMatrix.m[3][0] = 0.0f;
            runtime.emitMatrix.m[3][1] = 0.0f;
            runtime.emitMatrix.m[3][2] = 0.0f;
        }

        runtime.position[0] = matrix->m[3][0];
        runtime.position[1] = matrix->m[3][1];
        runtime.position[2] = matrix->m[3][2];
    }

    void AppendParticleEvent(ParticleEvent* event, ParticleEvent** list)
    {
        while (*list != nullptr)
        {
            list = &(*list)->next;
        }

        *list = event;
        event->next = nullptr;
    }

    void StartParticleEmitter(s32* runtimeIndex, s32 system, const f32* position, u32 cycles, ChunkData* chunk)
    {
        if (chunk == nullptr)
        {
            chunk = g_DrawnChunk;
        }

        CreateParticleEmitter(position[0], position[1], position[2], runtimeIndex, system, chunk);
        if (*runtimeIndex == -1)
        {
            return;
        }

        EmitterRuntime& runtime = g_EmitterRuntimes[*runtimeIndex];
        runtime.cyclesLeft = cycles;
        runtime.phase = 0;
        runtime.enabled = 1;
    }

    void CreateParticleEmitter(f32 x, f32 y, f32 z, s32* runtimeIndex, s32 systemIndex, ChunkData* chunk)
    {
        bool taken = false;
        if (systemIndex == -1)
        {
            return;
        }

        if (*runtimeIndex == -1)
        {
            taken = true;
            *runtimeIndex = AllocParticleEmitterSlot();
            if (*runtimeIndex == -1)
            {
                return;
            }
        }

        const ParticleSystem* system = g_LoadedParticleSystems[systemIndex];
        if (system == nullptr)
        {
            return;
        }

        EmitterRuntime* runtime = &g_EmitterRuntimes[*runtimeIndex];
        runtime->system = static_cast<s16>(systemIndex);
        runtime->enabled = 0;
        runtime->keepsEmitTranslation = 0;
        SetParticleEmitterTiming(*runtimeIndex, static_cast<s16>(system->timingOffset));
        runtime = &g_EmitterRuntimes[*runtimeIndex];
        runtime->onTimeLeft = static_cast<s16>(g_LoadedParticleSystems[systemIndex]->onTime);
        runtime->generator = g_ParticleGenerators[static_cast<s8>(system->genSort)];
        runtime->velocityRule = g_ParticleVelocityRules[static_cast<s8>(system->genCode)];
        runtime->cyclesLeft = 0;
        runtime->rotorYaw = 0;
        runtime->rotorTilt = 0;
        runtime->nextSphere = 0;
        runtime->sphereCountdown = 1;
        const ParticleSystem* current = g_LoadedParticleSystems[systemIndex];
        for (s32 sphere = 0; sphere < static_cast<s8>(current->collisionSpheres); sphere++)
        {
            g_EmitterRuntimes[*runtimeIndex].spheres[sphere].age = NoSphere;
        }

        runtime = &g_EmitterRuntimes[*runtimeIndex];
        for (u32 axis = 0; axis < 3; axis++)
        {
            runtime->startOffset[axis] = 0.0f;
            runtime->velocityOffset[axis] = 0.0f;
        }

        runtime->state = EmitterRuntime::StateLoose;
        runtime->chunk = chunk;
        runtime->cameraDistance = NotRunDistance;
        runtime->creationTime = g_ParticleTime;
        runtime->position[2] = z;
        runtime->position[0] = x;
        runtime->position[1] = y;
        runtime->emitMatrix = g_IdentityMatrix;
        MatrixRotateZ(&runtime->emitMatrix, 0);
        MatrixRotateY(&runtime->emitMatrix, 0);
        MatrixRotateX(&runtime->emitMatrix, 0);
        runtime->gravityMatrix = g_IdentityMatrix;
        MatrixRotateZ(&runtime->gravityMatrix, 0);
        MatrixRotateY(&runtime->gravityMatrix, 0);
        runtime->bounceFactor = DefaultBounceFactor;
        runtime->unusedShort = 0;
        runtime->bouncePlaneAngle = 0;
        runtime->planeOffset = 0.0f;
        // One that was running comes out of its wheel slot first
        if (!taken)
        {
            EmitterRuntime** slot = FindEmitterInWheel(runtime);
            if (slot != nullptr)
            {
                UnlinkEmitter(runtime, slot);
            }
        }

        EmitterRuntime*& first = g_ParticleEmitterWheel[g_ParticleEmitterWheelIndex];
        if (first != nullptr)
        {
            first->previous = runtime;
        }

        runtime->next = first;
        first = runtime;
    }

    void AllocParticleEmitterBlocks(EmitterRuntime* runtime)
    {
        s32 grow = runtime->blocksWanted - runtime->blockCount;
        ParticleSystem* system = g_LoadedParticleSystems[runtime->system];
        if (grow == 0)
        {
            return;
        }

        if (grow > 0)
        {
            bool hexagons = DrawsHexagons(system);
            s32& used = hexagons ? g_UsedHexagonBlocks : g_UsedParticleBlocks;
            if (grow + used >= (hexagons ? g_HexagonBlockCount : g_ParticleBlockCount))
            {
                return;
            }

            u8** freeBlocks = hexagons ? g_HexagonBlocks : g_ParticleBlocks;
            s32 firstFree = used;
            for (s32 block = 0; block < grow; block++)
            {
                runtime->blocks[block + runtime->blockCount] = freeBlocks[block + firstFree];
                ClearBlock(runtime->blocks[block + runtime->blockCount], hexagons ? HexagonBlockParticles : BlockParticles);
            }

            // Its first block gets an entry to draw it
            if (runtime->blockCount == 0)
            {
                for (ParticleDrawEntry* entry = g_ParticleDrawEntries; entry != EndOfDrawEntries(); entry++)
                {
                    if (entry->block != nullptr)
                    {
                        continue;
                    }

                    entry->system = system;
                    entry->block = runtime->blocks[0];
                    entry->owner = runtime;
                    if (static_cast<s8>(runtime->listsDraws) != 0)
                    {
                        ListDrawEntry(entry, system);
                    }

                    break;
                }
            }

            runtime->blockCount = runtime->blocksWanted;
            runtime->capacity = runtime->maxParticles;
            used += grow;
            Platform::Graphics::ChainParticleBlocks(runtime->blocks, runtime->blockCount);
            return;
        }

        if ((g_ParticleBlockCount + g_HexagonBlockCount) * EventsPerBlock < g_ParticleEventPoolTop - grow)
        {
            return;
        }

        for (s32 block = grow; block < 0; block++)
        {
            // Released once its last particles and ghosts are gone
            ParticleEvent* event = g_ParticleEventPool[g_ParticleEventPoolTop];
            event->block = runtime->blocks[block + runtime->blockCount];
            const ParticleSystem* current = g_LoadedParticleSystems[runtime->system];
            f32 life = current->lifeTime + static_cast<f32>(current->ghosts) * current->ghostSeparation;
            event->delay = static_cast<s32>(life * FramesPerSecond);
            event->kind = DrawsHexagons(system) ? ParticleEvent::DropHexagonDraw : ParticleEvent::DropParticleDraw;
            AppendParticleEvent(g_ParticleEventPool[g_ParticleEventPoolTop], &g_ParticleEventWheel[g_ParticleEventWheelIndex]);
            g_ParticleEventPoolTop++;
            if (block == -runtime->blockCount)
            {
                // All of them: the chain's entry draws on without the runtime
                for (ParticleDrawEntry* entry = g_ParticleDrawEntries; entry != EndOfDrawEntries(); entry++)
                {
                    if (entry->block == runtime->blocks[0])
                    {
                        TakeOverBlock(entry, runtime);
                        break;
                    }
                }

                continue;
            }

            // One of them: a chain of its own with an entry of its own
            u8* left = runtime->blocks[block + runtime->blockCount];
            Platform::Graphics::EndParticleBlock(left);
            for (ParticleDrawEntry* entry = g_ParticleDrawEntries; entry != EndOfDrawEntries(); entry++)
            {
                if (entry->block != nullptr)
                {
                    continue;
                }

                entry->block = runtime->blocks[block + runtime->blockCount];
                TakeOverBlock(entry, runtime);
                if (static_cast<s8>(runtime->listsDraws) != 0)
                {
                    ListDrawEntry(entry, g_LoadedParticleSystems[runtime->system]);
                }

                break;
            }
        }

        runtime->capacity = runtime->maxParticles;
        runtime->blockCount = runtime->blocksWanted;
        if (runtime->maxParticles == 0)
        {
            runtime->blocks[0] = nullptr;
            return;
        }

        Platform::Graphics::ChainParticleBlocks(runtime->blocks, runtime->blockCount);
    }

    void FreeParticleEmitterBlocks(s32* runtimeIndex)
    {
        if (*runtimeIndex == -1)
        {
            return;
        }

        for (s32 freeSlot = g_TakenEmitterRuntimes; freeSlot < static_cast<s32>(MaxEmitterRuntimes); freeSlot++)
        {
            if (g_FreeEmitterRuntimes[freeSlot] == *runtimeIndex)
            {
                *runtimeIndex = -1;
                return;
            }
        }

        ParticleSystem* system = g_LoadedParticleSystems[g_EmitterRuntimes[*runtimeIndex].system];
        if (g_EmitterRuntimes[*runtimeIndex].blockCount != 0)
        {
            if (g_EmitterRuntimes[*runtimeIndex].blockCount > 0)
            {
                s32 block = 0;
                do
                {
                    DropBlockEvents(g_EmitterRuntimes[*runtimeIndex].blocks[block]);
                    // Off the draw list once its last particles and ghosts are gone
                    ParticleEvent* event = g_ParticleEventPool[g_ParticleEventPoolTop];
                    event->block = g_EmitterRuntimes[*runtimeIndex].blocks[block];
                    const ParticleSystem* current = g_LoadedParticleSystems[g_EmitterRuntimes[*runtimeIndex].system];
                    f32 life = current->lifeTime + static_cast<f32>(current->ghosts) * current->ghostSeparation;
                    event->delay = static_cast<s32>(life * FramesPerSecond);
                    event->kind = DrawsHexagons(system) ? ParticleEvent::DropHexagonDraw : ParticleEvent::DropParticleDraw;
                    event->runtime = nullptr;
                    AppendParticleEvent(event, &g_ParticleEventWheel[g_ParticleEventWheelIndex]);
                    g_ParticleEventPoolTop++;
                    block++;
                } while (block < g_EmitterRuntimes[*runtimeIndex].blockCount);
            }

            for (ParticleDrawEntry* entry = g_ParticleDrawEntries; entry != EndOfDrawEntries(); entry++)
            {
                const EmitterRuntime& runtime = g_EmitterRuntimes[*runtimeIndex];
                if (entry->block != runtime.blocks[0])
                {
                    continue;
                }

                entry->system = system;
                entry->owner = nullptr;
                entry->matrix = runtime.gravityMatrix;
                entry->chunk = runtime.chunk;
                entry->position[0] = runtime.position[0];
                entry->position[1] = runtime.position[1];
                entry->position[2] = runtime.position[2];
                entry->texturePage = static_cast<s16>(system->texturePage);
                entry->keepsEmitTranslation = runtime.keepsEmitTranslation;
                break;
            }
        }

        EmitterRuntime** slot = FindEmitterInWheel(&g_EmitterRuntimes[*runtimeIndex]);
        if (slot != nullptr)
        {
            UnlinkEmitter(&g_EmitterRuntimes[*runtimeIndex], slot);
        }

        for (s16& emitterSlot : system->emitterSlots)
        {
            if (emitterSlot == *runtimeIndex)
            {
                emitterSlot = -1;
            }
        }

        g_TakenEmitterRuntimes--;
        g_EmitterRuntimes[*runtimeIndex].system = 0;
        g_FreeEmitterRuntimes[g_TakenEmitterRuntimes] = static_cast<s16>(*runtimeIndex);
        *runtimeIndex = -1;
    }

    void InitParticleBlocks(s32 blocks)
    {
        g_ParticleBlockCount = blocks;
        RetailLibc::MemorySet(g_ParticleEvents, 0, (blocks + g_HexagonBlockCount) * EventsPerBlock * sizeof(ParticleEvent));
        RetailLibc::MemorySet(g_ParticleEventPool, 0, (g_ParticleBlockCount + g_HexagonBlockCount) * EventsPerBlock * sizeof(ParticleEvent*));
        RetailLibc::MemorySet(g_ParticleBlocks, 0, g_ParticleBlockCount * sizeof(u8*));
        RetailLibc::MemorySet(g_HexagonBlocks, 0, g_HexagonBlockCount * sizeof(u8*));
        RetailLibc::MemorySet(g_ParticleDrawEntries, 0, (g_ParticleBlockCount + g_HexagonBlockCount) * sizeof(ParticleDrawEntry));
        auto* block = static_cast<u8*>(MemoryAllocate2(g_ParticleBlockCount * Platform::Graphics::ParticleBlockBytes));
        for (s32 index = 0; index < g_ParticleBlockCount; index++)
        {
            Platform::Graphics::InitParticleBlock(block, false);
            g_ParticleBlocks[index] = block;
            block += Platform::Graphics::ParticleBlockBytes;
        }

        block = static_cast<u8*>(MemoryAllocate2(g_HexagonBlockCount * Platform::Graphics::HexagonBlockBytes));
        for (s32 index = 0; index < g_HexagonBlockCount; index++)
        {
            Platform::Graphics::InitParticleBlock(block, true);
            g_HexagonBlocks[index] = block;
            block += Platform::Graphics::HexagonBlockBytes;
        }

        g_UsedParticleBlocks = 0;
        g_UsedHexagonBlocks = 0;
        auto* table = static_cast<u8*>(MemoryAllocate2(MaxParticleSystems * Platform::Graphics::ParticleRenderTableBytes));
        for (u32 index = 0; index < MaxParticleSystems; index++)
        {
            Platform::Graphics::InitParticleRenderTable(table);
            g_RenderTablePool[index] = table;
            table += Platform::Graphics::ParticleRenderTableBytes;
        }

        for (s32 frame = ParticleWheelFrames - 1; frame >= 0; frame--)
        {
            g_ParticleEventWheel[frame] = nullptr;
        }

        g_ParticleEventWheelIndex = 0;
        for (s32 index = 0; index < (g_ParticleBlockCount + g_HexagonBlockCount) * EventsPerBlock; index++)
        {
            g_ParticleEventPool[index] = &g_ParticleEvents[index];
        }

        g_ParticleEventPoolTop = 0;
        for (ParticleDrawEntry* entry = g_ParticleDrawEntries; entry != EndOfDrawEntries(); entry++)
        {
            entry->block = nullptr;
            entry->system = nullptr;
            entry->owner = nullptr;
            entry->previous = nullptr;
            entry->next = nullptr;
        }

        for (s32 list = ParticleDrawListCount - 1; list >= 0; list--)
        {
            g_ParticleDrawLists[list] = nullptr;
        }
    }

    void UpdateParticleEmitterTiming()
    {
        EmitterRuntime* runtime = g_ParticleEmitterWheel[g_ParticleEmitterWheelIndex];
        while (runtime != nullptr)
        {
            EmitterRuntime* next = runtime->next;
            const ParticleSystem* system = g_LoadedParticleSystems[runtime->system];
            if (runtime->phase != 0)
            {
                runtime = next;
                continue;
            }

            runtime->onTimeLeft = static_cast<s16>(runtime->onTimeLeft - 1);
            if (runtime->enabled == 0 && (static_cast<s16>(system->offTime) == 0 || static_cast<s16>(system->offTimeRandom) != 0))
            {
                s32 random = RandomNext(nullptr);
                runtime->phase = static_cast<s16>(random % RestPhaseSpread + RestPhaseBase);
            }
            else
            {
                runtime->phase = 1;
            }

            if (runtime->onTimeLeft != 0)
            {
                runtime = next;
                continue;
            }

            runtime->phase = static_cast<s16>(runtime->phase + system->offTime);
            if (static_cast<s16>(system->offTimeRandom) != 0)
            {
                s32 random = RandomNext(nullptr);
                runtime->phase = static_cast<s16>(runtime->phase + 1 + random % static_cast<s16>(system->offTimeRandom));
            }

            runtime->onTimeLeft = static_cast<s16>(system->onTime);
            if (static_cast<s16>(system->onTimeRandom) != 0)
            {
                s32 random = RandomNext(nullptr);
                runtime->onTimeLeft = static_cast<s16>(runtime->onTimeLeft + 1 + random % static_cast<s16>(system->onTimeRandom));
            }

            u32 cycles = runtime->cyclesLeft;
            if (cycles != 0)
            {
                runtime->cyclesLeft = cycles - 1;
                if (cycles - 1 == 0)
                {
                    for (s32 index = 0; index < static_cast<s32>(MaxEmitterRuntimes); index++)
                    {
                        if (&g_EmitterRuntimes[index] == runtime)
                        {
                            s32 released = index;
                            FreeParticleEmitterBlocks(&released);
                            break;
                        }
                    }

                    runtime->phase = ReleasedPhase;
                }
            }

            runtime = next;
        }
    }

    void RescheduleParticleEmitters()
    {
        s32 current = g_ParticleEmitterWheelIndex;
        EmitterRuntime* runtime = g_ParticleEmitterWheel[current];
        while (runtime != nullptr)
        {
            EmitterRuntime* next = runtime->next;
            if (static_cast<s8>(g_LoadedParticleSystems[runtime->system]->collisionSpheres) != 0)
            {
                UnlinkEmitter(runtime, &g_ParticleEmitterWheel[current]);
                PushEmitter(runtime, &g_ParticleEmitterWheel[(current + 1) & (ParticleWheelFrames - 1)]);
                runtime->phase = static_cast<s16>(runtime->phase - 1);
            }
            else if (runtime->phase >= static_cast<s16>(ParticleWheelFrames))
            {
                runtime->phase = static_cast<s16>(runtime->phase - ParticleWheelFrames);
            }
            else
            {
                UnlinkEmitter(runtime, &g_ParticleEmitterWheel[current]);
                PushEmitter(runtime, &g_ParticleEmitterWheel[(current + runtime->phase) & (ParticleWheelFrames - 1)]);
                runtime->phase = 0;
            }

            runtime = next;
        }

        g_ParticleEmitterWheelIndex = (g_ParticleEmitterWheelIndex + 1) & (ParticleWheelFrames - 1);
    }

    void ProcessParticleBlockEvents()
    {
        ParticleEvent* event = g_ParticleEventWheel[g_ParticleEventWheelIndex];
        while (event != nullptr)
        {
            ParticleEvent* next = event->next;
            // Due in a later turn of the wheel, or in a frame of this one
            if (event->delay != 0)
            {
                if (event->delay >= static_cast<s32>(ParticleWheelFrames))
                {
                    event->delay -= ParticleWheelFrames;
                }
                else
                {
                    TakeOutOfFrame(event);
                    MoveToFrame(event, event->delay);
                }

                event = next;
                continue;
            }

            switch (event->kind)
            {
                case ParticleEvent::PlaneBounce:
                {
                    // The particle bounced at the event's time: its height and upward velocity made the bounce's, and the time it
                    // comes down onto the plane again worked out
                    const ParticleSystem* system = g_LoadedParticleSystems[event->system];
                    auto* record = reinterpret_cast<ParticleRecord*>(event->block + ParticleBlockHeader) + event->record;
                    f32 pull = event->time * system->gravity;
                    pull = pull + pull;
                    f32 travelled = record->velocity[1] * event->time;
                    f32 velocity = -event->bounceFactor * (record->velocity[1] + pull) - pull;
                    record->velocity[1] = velocity;
                    f32 height = record->start[1] - (velocity * event->time - travelled);
                    record->start[1] = height;
                    f32 first;
                    f32 second;
                    if (PlaneTimes(height - event->planeOffset, velocity, system->gravity, &first, &second))
                    {
                        f32 latest = Platform::Math::Max(first, second);
                        if (latest < system->lifeTime)
                        {
                            f32 before = event->time;
                            event->time = latest;
                            event->delay = static_cast<s32>((latest - before) * FramesPerSecond);
                            if (event->delay >= static_cast<s32>(ParticleWheelFrames))
                            {
                                event->delay -= ParticleWheelFrames;
                                break;
                            }

                            if (event->delay != 0)
                            {
                                TakeOutOfFrame(event);
                                MoveToFrame(event, event->delay);
                                break;
                            }

                            // Coming down within the frame: it rests on the plane
                            if (second != first)
                            {
                                record->velocity[1] = 0.0f;
                                record->start[1] = event->planeOffset;
                            }
                        }
                    }

                    TakeOutOfFrame(event);
                    ReleaseEvent(event);
                    break;
                }
                case ParticleEvent::WallBounce:
                {
                    // The particle's flat velocity bounced off the vertical plane, its start moved so it's where it was at the
                    // event's time
                    Vector4 normal = {1.0f, 0.0f, 0.0f, 0.0f};
                    auto* record = reinterpret_cast<ParticleRecord*>(event->block + ParticleBlockHeader) + event->record;
                    RotateVectorY(&normal, &normal, event->planeAngle);
                    f32 velocityX = record->velocity[0];
                    f32 velocityZ = record->velocity[2];
                    f32 travelledZ = velocityZ * event->time;
                    f32 travelledX = velocityX * event->time;
                    f32 push = (velocityX * normal.x + velocityZ * normal.z) * -(event->bounceFactor + 1.0f);
                    velocityX = velocityX + push * normal.x;
                    record->velocity[0] = velocityX;
                    velocityZ = velocityZ + push * normal.z;
                    record->velocity[2] = velocityZ;
                    record->start[2] = record->start[2] - (velocityZ * event->time - travelledZ);
                    record->start[0] = record->start[0] - (velocityX * event->time - travelledX);
                    TakeOutOfFrame(event);
                    ReleaseEvent(event);
                    break;
                }
                case ParticleEvent::FreeParticleBlock:
                    g_UsedParticleBlocks--;
                    g_ParticleBlocks[g_UsedParticleBlocks] = event->block;
                    FreeBlock(event->block, BlockParticles);
                    TakeOutOfFrame(event);
                    ReleaseEvent(event);
                    break;
                case ParticleEvent::FreeHexagonBlock:
                    g_UsedHexagonBlocks--;
                    g_HexagonBlocks[g_UsedHexagonBlocks] = event->block;
                    FreeBlock(event->block, HexagonBlockParticles);
                    TakeOutOfFrame(event);
                    ReleaseEvent(event);
                    break;
                case ParticleEvent::EndParticleChain:
                case ParticleEvent::EndHexagonChain:
                    Platform::Graphics::EndParticleBlock(event->block);
                    TakeOutOfFrame(event);
                    MoveToFrame(event, ReleaseDelay);
                    event->kind = event->kind == ParticleEvent::EndParticleChain ? ParticleEvent::FreeParticleBlock : ParticleEvent::FreeHexagonBlock;
                    break;
                case ParticleEvent::DropParticleDraw:
                case ParticleEvent::DropHexagonDraw:
                    for (ParticleDrawEntry* entry = g_ParticleDrawEntries; entry != EndOfDrawEntries(); entry++)
                    {
                        if (entry->block != event->block)
                        {
                            continue;
                        }

                        UnlinkDrawEntry(entry, entry->system);
                        entry->owner = nullptr;
                        entry->block = nullptr;
                        entry->system = nullptr;
                        break;
                    }

                    TakeOutOfFrame(event);
                    MoveToFrame(event, ReleaseDelay);
                    event->kind = event->kind == ParticleEvent::DropParticleDraw ? ParticleEvent::EndParticleChain : ParticleEvent::EndHexagonChain;
                    break;
                case ParticleEvent::BlockTaken:
                {
                    // The block out of its runtime's: the ones after it moved down, the runtime's particles fewer (its next slot
                    // too when it was past the block)
                    EmitterRuntime* runtime = event->runtime;
                    ParticleSystem* system = g_LoadedParticleSystems[runtime->system];
                    s32 taken = 0;
                    for (s32 block = 0; block < runtime->blockCount; block++)
                    {
                        if (runtime->blocks[block] == event->block)
                        {
                            taken = block;
                            break;
                        }
                    }

                    MoveBlocksDown(runtime, taken);
                    runtime->blocks[runtime->blockCount - 1] = nullptr;
                    runtime->blockCount--;
                    runtime->blocksWanted--;
                    runtime->unused108--;
                    if (runtime->blockCount <= 0)
                    {
                        runtime->capacity = 0;
                        runtime->maxParticles = 0;
                    }
                    else
                    {
                        s32 particles = DrawsHexagons(system) ? HexagonBlockParticles : BlockParticles;
                        runtime->capacity = static_cast<s16>(runtime->capacity - particles);
                        runtime->maxParticles = static_cast<s16>(runtime->maxParticles - particles);
                        if ((taken + 1) * particles < runtime->nextSlot)
                        {
                            runtime->nextSlot = static_cast<s16>(runtime->nextSlot - particles);
                        }
                    }

                    if (runtime->blockCount > 0)
                    {
                        Platform::Graphics::ChainParticleBlocks(runtime->blocks, runtime->blockCount);
                    }
                    else
                    {
                        // Its last one: the runtime made free
                        for (s16& slot : system->emitterSlots)
                        {
                            if (slot != -1 && &g_EmitterRuntimes[slot] == runtime)
                            {
                                runtime->system = 0;
                                g_TakenEmitterRuntimes--;
                                g_FreeEmitterRuntimes[g_TakenEmitterRuntimes] = slot;
                                slot = -1;
                            }
                        }

                        runtime->blocks[0] = nullptr;
                    }

                    // The chain's first block: its entry draws the new first, or leaves its list with none
                    if (taken == 0)
                    {
                        for (ParticleDrawEntry* entry = g_ParticleDrawEntries; entry != EndOfDrawEntries(); entry++)
                        {
                            if (entry->block != event->block)
                            {
                                continue;
                            }

                            entry->block = runtime->blocks[0];
                            if (runtime->blockCount == 0)
                            {
                                UnlinkDrawEntry(entry, system);
                                entry->owner = nullptr;
                                entry->system = nullptr;
                            }

                            break;
                        }
                    }

                    TakeOutOfFrame(event);
                    MoveToFrame(event, ReleaseDelay);
                    event->kind = DrawsHexagons(system) ? ParticleEvent::EndHexagonChain : ParticleEvent::EndParticleChain;
                    break;
                }
                default:
                    break;
            }

            event = next;
        }

        g_ParticleEventWheelIndex = (g_ParticleEventWheelIndex + 1) & (ParticleWheelFrames - 1);
    }

    void UpdateParticleCollisionSpheres()
    {
        EmitterRuntime* runtime = g_ParticleEmitterWheel[g_ParticleEmitterWheelIndex];
        while (runtime != nullptr)
        {
            ParticleSystem* system = g_LoadedParticleSystems[runtime->system];
            EmitterRuntime* next = runtime->next;
            if (static_cast<s8>(system->collisionSpheres) == 0)
            {
                runtime = next;
                continue;
            }

            g_CollidingSystems[g_CollidingCount] = system;
            g_CollidingRuntimes[g_CollidingCount] = runtime;
            g_CollidingCount++;
            for (s32 sphere = 0; sphere < static_cast<s8>(system->collisionSpheres); sphere++)
            {
                f32& age = runtime->spheres[sphere].age;
                if (age == NoSphere)
                {
                    continue;
                }

                age = age + g_ParticleFrameDelta;
                if (system->lifeTime < age)
                {
                    age = NoSphere;
                }
            }

            runtime->sphereCountdown = static_cast<s16>(runtime->sphereCountdown - 1);
            if (runtime->phase != 0 || runtime->enabled == 0 || runtime->sphereCountdown > 0)
            {
                runtime = next;
                continue;
            }

            CollisionSphere& sphere = runtime->spheres[runtime->nextSphere];
            sphere.age = 0.0f;
            Vector4 velocity = {0.0f, system->velocity, 0.0f, 0.0f};
            TransformVector(&velocity, &velocity, &runtime->emitMatrix);
            sphere.velocity[0] = velocity.x;
            sphere.velocity[1] = velocity.y;
            sphere.velocity[2] = velocity.z;
            runtime->nextSphere = static_cast<s16>(runtime->nextSphere + 1);
            if (runtime->nextSphere >= static_cast<s8>(system->collisionSpheres))
            {
                runtime->nextSphere = 0;
            }

            f32 apart = system->lifeTime * FramesPerSecond / static_cast<f32>(static_cast<s8>(system->collisionSpheres));
            runtime->sphereCountdown = static_cast<s16>(static_cast<s32>(apart));
            runtime = next;
        }
    }

    s32 TestParticleCollisionSpheres(const f32* point, f32 radius, f32 verticalScale)
    {
        if (g_CollidingCount == 0)
        {
            return -1;
        }

        f32 inverseScale = 1.0f / verticalScale;
        for (s32 entry = 0; entry < g_CollidingCount; entry++)
        {
            const ParticleSystem* system = g_CollidingSystems[entry];
            EmitterRuntime* runtime = g_CollidingRuntimes[entry];
            for (s32 index = 0; index < static_cast<s8>(system->collisionSpheres); index++)
            {
                const CollisionSphere& sphere = runtime->spheres[index];
                f32 age = sphere.age;
                f32 sphereRadius = 0.0f;
                if (age != NoSphere)
                {
                    Vector4 velocity = {sphere.velocity[0], sphere.velocity[1], sphere.velocity[2], 0.0f};
                    TransformVector(&velocity, &velocity, &runtime->gravityMatrix);
                    g_CollisionSphereCentre.x = velocity.x * age + runtime->position[0];
                    g_CollisionSphereCentre.y = velocity.y * age + system->gravity * (age * age) + runtime->position[1];
                    g_CollisionSphereCentre.z = velocity.z * age + runtime->position[2];
                    sphereRadius = SampleCurve(system->collisionRadiusKeys, age / system->lifeTime);
                }

                if (sphereRadius <= 0.0f)
                {
                    continue;
                }

                f32 reach = sphereRadius + radius;
                f32 x = point[0] - g_CollisionSphereCentre.x;
                f32 y = point[1] - g_CollisionSphereCentre.y;
                f32 z = point[2] - g_CollisionSphereCentre.z;
                if (inverseScale != 1.0f)
                {
                    y = y * (reach / (sphereRadius + radius * inverseScale));
                }

                if (x * x + y * y + z * z < reach * reach)
                {
                    return entry;
                }
            }
        }

        return -1;
    }

    s32 TestParticleCollision(const f32* point, f32 radius)
    {
        return TestParticleCollisionSpheres(point, radius, 1.0f);
    }

    ParticleRecord* GenParticle_Box(EmitterRuntime* runtime, ParticleSystem* system)
    {
        WrapSlot(runtime);
        ParticleRecord* record = TakeRecord(runtime, BlockSize(system));
        StartRecord(record, system);
        BoxStart(runtime, system, record);
        BoxVelocity(runtime, system, record);
        ApplyVelocityRule(runtime, system, record);
        AddOffsets(runtime, record);
        AddGhosts(runtime, system, record);
        return EndRecord(system, record);
    }

    ParticleRecord* GenParticle_Box2(EmitterRuntime* runtime, ParticleSystem* system)
    {
        return GenParticle_Box(runtime, system);
    }

    ParticleRecord* GenParticle_Radial(EmitterRuntime* runtime, ParticleSystem* system)
    {
        WrapSlot(runtime);
        if (DrawsHexagons(system))
        {
            return nullptr;
        }

        ParticleRecord* record = TakeRecord(runtime, BlockSize(system));
        StartRecord(record, system);
        s32 yaw = static_cast<s32>(RandomSpread(system->randomEmit[RadialYaw]) + system->randomStart[RadialYaw]);
        s32 tilt = static_cast<s32>(RandomSpread(system->randomEmit[RadialTilt]) + system->randomStart[RadialTilt]);
        RadialStart(runtime, system, record, RampedRadius(runtime, system), tilt, yaw);
        ApplyVelocityRule(runtime, system, record);
        AddOffsets(runtime, record);
        AddGhosts(runtime, system, record);
        return EndRecord(system, record);
    }

    ParticleRecord* GenParticle_RadialRotor(EmitterRuntime* runtime, ParticleSystem* system)
    {
        WrapSlot(runtime);
        if (DrawsHexagons(system))
        {
            return nullptr;
        }

        ParticleRecord* record = TakeRecord(runtime, BlockSize(system));
        StartRecord(record, system);
        s32 tilt = static_cast<s32>(static_cast<f32>(runtime->rotorTilt) + system->randomStart[RadialTilt]);
        s32 yaw = static_cast<s32>(static_cast<f32>(runtime->rotorYaw) + system->randomStart[RadialYaw]);
        RadialStart(runtime, system, record, system->randomStart[RadialRadius], tilt, yaw);
        ApplyVelocityRule(runtime, system, record);
        AddOffsets(runtime, record);
        AddGhosts(runtime, system, record);
        EndRecord(system, record);
        // The rotor turns on by the yaw's and tilt's spreads (none: back to 0)
        if (system->randomEmit[RadialYaw] == 0.0f)
        {
            runtime->rotorYaw = 0;
        }
        else
        {
            runtime->rotorYaw = static_cast<s16>(runtime->rotorYaw + static_cast<s32>(system->randomEmit[RadialYaw]));
        }

        if (system->randomEmit[RadialTilt] == 0.0f)
        {
            runtime->rotorTilt = 0;
        }
        else
        {
            runtime->rotorTilt = static_cast<s16>(runtime->rotorTilt + static_cast<s32>(system->randomEmit[RadialTilt]));
        }

        return record;
    }

    ParticleRecord* GenParticle_Spheroid(EmitterRuntime* runtime, ParticleSystem* system)
    {
        WrapSlot(runtime);
        ParticleRecord* record = TakeRecord(runtime, BlockSize(system));
        StartRecord(record, system);
        Vector4 point = {__builtin_sqrtf(RandomFloat01(&g_ParticleRandom)), 0.0f, 0.0f, 0.0f};
        // The yaw it picks: the random number's whole part, always 0, in turns
        s32 yaw = static_cast<s32>(RandomFloat01(&g_ParticleRandom)) * FullTurnAngle;
        f32 random = RandomFloat01(&g_ParticleRandom);
        s32 angle;
        AngleOfSine(random + random - 1.0f, &angle);
        RotateVectorZ(&point, &point, ThroughRadians(angle));
        RotateVectorY(&point, &point, yaw);
        Matrix4x4 scale;
        MatrixIdentity(&scale);
        scale.m[0][0] = system->randomStart[0];
        scale.m[1][1] = system->randomStart[1];
        scale.m[2][2] = system->randomStart[2];
        TransformPoint(&point, &point, &scale);
        TransformPoint(&point, &point, &runtime->emitMatrix);
        StoreVector(record->start, point);
        BoxVelocity(runtime, system, record);
        ApplyVelocityRule(runtime, system, record);
        AddOffsets(runtime, record);
        AddGhosts(runtime, system, record);
        return EndRecord(system, record);
    }

    ParticleRecord* GenParticle_Bounce(EmitterRuntime* runtime, ParticleSystem* system)
    {
        WrapSlot(runtime);
        if (DrawsHexagons(system))
        {
            return nullptr;
        }

        s32 slot = runtime->nextSlot;
        u8* block = runtime->blocks[slot / BlockParticles];
        ParticleRecord* record = RecordAt(runtime, slot, BlockParticles);
        StartRecord(record, system);
        BoxStart(runtime, system, record);
        BoxVelocity(runtime, system, record);
        // Its bounce queued for when it comes down onto the emitter's plane within its life, while half the events are free
        f32 first;
        f32 second;
        if (PlaneTimes(record->start[1] - runtime->planeOffset, record->velocity[1], system->gravity, &first, &second))
        {
            f32 latest = Platform::Math::Max(first, second);
            if (0.0f < latest && latest < system->lifeTime && g_ParticleEventPoolTop < g_ParticleBlockCount + g_HexagonBlockCount)
            {
                ParticleEvent* event = g_ParticleEventPool[g_ParticleEventPoolTop];
                event->block = block;
                event->delay = static_cast<s32>(latest * FramesPerSecond);
                event->kind = ParticleEvent::PlaneBounce;
                event->runtime = nullptr;
                event->system = runtime->system;
                event->planeAngle = 0;
                event->unused1E = 0;
                event->planeOffset = runtime->planeOffset;
                event->bounceFactor = runtime->bounceFactor;
                event->record = slot % BlockParticles;
                event->time = latest;
                AppendParticleEvent(event, &g_ParticleEventWheel[g_ParticleEventWheelIndex]);
                g_ParticleEventPoolTop++;
            }
        }

        ApplyVelocityRule(runtime, system, record);
        runtime->nextSlot = static_cast<s16>(runtime->nextSlot + 1);
        AddOffsets(runtime, record);
        return EndRecord(system, record);
    }

    ParticleRecord* GenParticle_Sphere(EmitterRuntime* runtime, ParticleSystem* system)
    {
        WrapSlot(runtime);
        if (DrawsHexagons(system))
        {
            return nullptr;
        }

        ParticleRecord* record = TakeRecord(runtime, BlockSize(system));
        StartRecord(record, system);
        s32 yawRandom = static_cast<s32>(RandomFloat01(&g_ParticleRandom) * UnitsPerTurn) - HalfTurnAngle;
        // Even over the area: the sine of the tilt picked between the two tilts' sines
        f32 sines[4];
        SinCos16Pair(static_cast<s32>(system->randomStart[RadialTilt] - system->randomEmit[RadialTilt]),
                     static_cast<s32>(system->randomStart[RadialTilt] + system->randomEmit[RadialTilt]), sines);
        f32 low = sines[0];
        f32 high = sines[2];
        f32 random = RandomFloat01(&g_ParticleRandom);
        s32 angle;
        AngleOfSine(low + random * (high - low), &angle);
        s32 tilt = ThroughRadians(angle);
        s32 yaw = static_cast<s32>(static_cast<f32>(yawRandom) * system->randomEmit[RadialYaw]) / HalfTurnAngle +
                  static_cast<s32>(system->randomStart[RadialYaw]);
        Vector4 point = {system->randomStart[RadialRadius], 0.0f, 0.0f, 0.0f};
        RotateVectorZ(&point, &point, tilt);
        RotateVectorY(&point, &point, yaw);
        TransformPoint(&point, &point, &runtime->emitMatrix);
        StoreVector(record->start, point);
        Vector4 velocity = {RandomSpread(system->randomEmit[RadialSpeed]) + system->velocity, 0.0f, 0.0f, 0.0f};
        RotateVectorZ(&velocity, &velocity, tilt);
        RotateVectorY(&velocity, &velocity, yaw);
        TransformVector(&velocity, &velocity, &runtime->emitMatrix);
        StoreVector(record->velocity, velocity);
        ApplyVelocityRule(runtime, system, record);
        AddOffsets(runtime, record);
        AddGhosts(runtime, system, record);
        return EndRecord(system, record);
    }

    ParticleRecord* GenParticle_Ranges(EmitterRuntime* runtime, ParticleSystem* system)
    {
        WrapSlot(runtime);
        ParticleRecord* record = TakeRecord(runtime, BlockParticles);
        StartRangeRecord(record, system);
        record->start[0] = RandomRange(system->startRandomScale[0], system->startBase[0]);
        record->start[1] = RandomRange(system->startRandomScale[1], system->startBase[1]);
        record->start[2] = RandomRange(system->startRandomScale[2], system->startBase[2]);
        record->velocity[0] = RandomRange(system->velocityRandomScale[0], system->velocityBase[0]);
        record->velocity[1] = RandomRange(system->velocityRandomScale[1], system->velocityBase[1]);
        record->velocity[2] = RandomRange(system->velocityRandomScale[2], system->velocityBase[2]);
        ApplyVelocityRule(runtime, system, record);
        return EndRecord(system, record);
    }

    ParticleRecord* GenParticle_RangesRandomLife(EmitterRuntime* runtime, ParticleSystem* system)
    {
        WrapSlot(runtime);
        ParticleRecord* record = TakeRecord(runtime, BlockParticles);
        record->spawnTime = g_ParticleTime;
        record->lifeScale = RandomLifeScale(system);
        record->start[0] = RandomRange(system->startRandomScale[0], system->startBase[0]);
        record->start[1] = RandomRange(system->startRandomScale[1], system->startBase[1]);
        record->start[2] = RandomRange(system->startRandomScale[2], system->startBase[2]);
        record->velocity[0] = RandomRange(system->velocityRandomScale[0], system->velocityBase[0]);
        record->velocity[1] = RandomRange(system->velocityRandomScale[1], system->velocityBase[1]);
        record->velocity[2] = RandomRange(system->velocityRandomScale[2], system->velocityBase[2]);
        ApplyVelocityRule(runtime, system, record);
        return EndRecord(system, record);
    }

    ParticleRecord* GenParticle_Line(EmitterRuntime* runtime, ParticleSystem* system)
    {
        // Up to 0.3 either way of rand's 2^31 along the line, 1 to 2.5 along the other
        constexpr f32 AlongScale = 0x1.333334p-32f;
        constexpr f32 AlongReach = 0x1.333334p-2f;
        constexpr f32 OutScale = 0x1.8p-31f;
        constexpr f32 OutBase = 1.0f;
        constexpr f32 OutAlong = 1.5f;
        constexpr s32 LineAngle = 0x36B0;
        constexpr s32 OutAngle = 0x7530;
        WrapSlot(runtime);
        ParticleRecord* record = TakeRecord(runtime, BlockParticles);
        StartRangeRecord(record, system);
        f32 along = static_cast<f32>(GetRand()) * AlongScale - AlongReach;
        record->start[0] = along * Sin16(LineAngle);
        record->start[2] = along * Cos16(LineAngle);
        f32 out = static_cast<f32>(GetRand()) * OutScale + OutBase;
        record->velocity[0] = record->start[0] * OutAlong + out * Sin16(OutAngle);
        record->velocity[2] = record->start[2] * OutAlong + out * Cos16(OutAngle);
        record->start[1] = RandomRange(system->startRandomScale[1], system->startBase[1]);
        record->velocity[1] = RandomRange(system->velocityRandomScale[1], system->velocityBase[1]);
        ApplyVelocityRule(runtime, system, record);
        return EndRecord(system, record);
    }

    ParticleRecord* GenParticle_Reuse(EmitterRuntime* runtime, ParticleSystem* system)
    {
        WrapSlot(runtime);
        ParticleRecord* record = TakeRecord(runtime, BlockParticles);
        StartRangeRecord(record, system);
        // Three random numbers asked for and dropped
        GetRand();
        GetRand();
        GetRand();
        record->velocity[0] = record->start[0] + record->start[0];
        record->velocity[2] = record->start[2] + record->start[2];
        record->start[1] = RandomRange(system->startRandomScale[1], system->startBase[1]);
        record->velocity[1] = RandomRange(system->velocityRandomScale[1], system->velocityBase[1]);
        ApplyVelocityRule(runtime, system, record);
        return EndRecord(system, record);
    }

    ParticleRecord* GenParticle_BounceXZ(EmitterRuntime* runtime, ParticleSystem* system)
    {
        Vector4 normal = {1.0f, 0.0f, 0.0f, 0.0f};
        WrapSlot(runtime);
        if (DrawsHexagons(system))
        {
            return nullptr;
        }

        s32 slot = runtime->nextSlot;
        u8* block = runtime->blocks[slot / BlockParticles];
        ParticleRecord* record = RecordAt(runtime, slot, BlockParticles);
        StartRecord(record, system);
        BoxStart(runtime, system, record);
        BoxVelocity(runtime, system, record);
        // Its bounce queued when its flat path over its life goes through the plane, while half the events are free
        RotateVectorY(&normal, &normal, runtime->bouncePlaneAngle);
        Vector4 plane;
        ScaleVector(&plane, &normal, runtime->planeOffset);
        f32 endX = record->start[0] + system->lifeTime * record->velocity[0];
        f32 endZ = record->start[2] + system->lifeTime * record->velocity[2];
        f32 before = (plane.x - record->start[0]) * normal.x + (plane.z - record->start[2]) * normal.z;
        f32 after = (plane.x - endX) * normal.x + (plane.z - endZ) * normal.z;
        if ((before < 0.0f && 0.0f < after) || (0.0f < before && after < 0.0f))
        {
            f32 time = system->lifeTime * (-before / (after - before));
            if (g_ParticleEventPoolTop < g_ParticleBlockCount + g_HexagonBlockCount)
            {
                ParticleEvent* event = g_ParticleEventPool[g_ParticleEventPoolTop];
                event->block = block;
                event->delay = static_cast<s32>(time * FramesPerSecond);
                event->kind = ParticleEvent::WallBounce;
                event->runtime = nullptr;
                event->system = runtime->system;
                event->planeAngle = runtime->bouncePlaneAngle;
                event->unused1E = 0;
                event->planeOffset = runtime->planeOffset;
                event->bounceFactor = runtime->bounceFactor;
                event->record = slot % BlockParticles;
                event->time = time;
                AppendParticleEvent(event, &g_ParticleEventWheel[g_ParticleEventWheelIndex]);
                g_ParticleEventPoolTop++;
            }
        }

        ApplyVelocityRule(runtime, system, record);
        runtime->nextSlot = static_cast<s16>(runtime->nextSlot + 1);
        AddOffsets(runtime, record);
        return EndRecord(system, record);
    }

    ParticleRecord* GenParticle_Star(EmitterRuntime* runtime, ParticleSystem* system)
    {
        constexpr f32 HalfTurnUnits = HalfTurnAngle;
        WrapSlot(runtime);
        if (DrawsHexagons(system))
        {
            return nullptr;
        }

        ParticleRecord* record = TakeRecord(runtime, BlockParticles);
        StartRecord(record, system);
        s32 yaw = static_cast<s32>(RandomSpread(system->randomEmit[RadialYaw]) + system->randomStart[RadialYaw]);
        s32 tilt = static_cast<s32>(RandomSpread(system->randomEmit[RadialTilt]) + system->randomStart[RadialTilt]);
        f32 radius = RampedRadius(runtime, system);
        // Where the yaw is between two points: half a turn of a sine from one to the next, the radius at the ratio between them
        s32 spoke = FullTurnAngle / system->starPoints;
        f32 between = Sin16(static_cast<s32>(static_cast<f32>((yaw + FullTurnAngle) % spoke) / static_cast<f32>(spoke) * HalfTurnUnits));
        radius = radius * (system->starRadiusRatio + (1.0f - system->starRadiusRatio) * (1.0f - between));
        RadialStart(runtime, system, record, radius, tilt, yaw);
        ApplyVelocityRule(runtime, system, record);
        AddOffsets(runtime, record);
        AddGhosts(runtime, system, record);
        return EndRecord(system, record);
    }

    s32 DrawParticleList(s32, s32 list, ParticleViews* views)
    {
        if (g_ParticlesOn == 0 || g_ParticlesSetUp == 0)
        {
            return 0;
        }

        s32 modes = list == DrawListDistortion ? g_DistortionModeCount : g_ParticleModeCount;
        s32 order[4];
        for (s32 mode = 0; mode < modes; mode++)
        {
            order[mode] = list != DrawListDistortion ? g_ParticleModeOrder[mode] : g_DistortionModeOrder[mode];
        }

        s32 visited = 0;
        for (s32 mode = 0; mode < modes; mode++)
        {
            for (ParticleDrawEntry* entry = g_ParticleDrawLists[list]; entry != nullptr; entry = entry->next)
            {
                visited++;
                if (entry->block == nullptr)
                {
                    continue;
                }

                ParticleSystem* system = entry->system;
                if (system->renderTable == nullptr || static_cast<s8>(system->blendMode) != order[mode])
                {
                    continue;
                }

                // A runtime's own block draws with what the runtime has now, one let go of with what it had
                EmitterRuntime* owner = entry->owner;
                const Matrix4x4* gravity;
                const f32* place;
                ChunkData* chunk;
                s32 page;
                bool keepsTranslation;
                if (owner != nullptr)
                {
                    gravity = &owner->gravityMatrix;
                    place = owner->position;
                    chunk = owner->chunk;
                    page = system->texturePage;
                    keepsTranslation = static_cast<s8>(owner->keepsEmitTranslation) != 0;
                }
                else
                {
                    gravity = &entry->matrix;
                    place = entry->position;
                    chunk = entry->chunk;
                    page = entry->texturePage;
                    keepsTranslation = static_cast<s8>(entry->keepsEmitTranslation) != 0;
                }

                s32 view = ParticleViewOf(views, chunk);
                if (view == -1)
                {
                    continue;
                }

                Matrix4x4 matrix;
                f32 distance;
                if (Platform::Math::ParticleBlockView(view, keepsTranslation, system->boundingExtents, gravity, place, system->scaleFactor,
                                                      &matrix, &distance))
                {
                    continue;
                }

                if (!(distance < system->drawCutOff))
                {
                    continue;
                }

                if (DrawsHexagons(system))
                {
                    Platform::Graphics::DrawDistortionBlock(entry->block, entry->system->renderTable, &matrix, &g_DistortionMaterial,
                                                            g_ParticleTime, system->distortionX, system->distortionY);
                    continue;
                }

                Material* material = g_ParticlePages[page].materials[static_cast<s8>(entry->system->blendMode)];
                Platform::Graphics::DrawParticleBlock(entry->block, entry->system->renderTable, &matrix, material, g_ParticleTime);
            }
        }

        return visited;
    }

    void InitParticles(s32 initialise, s32 priority)
    {
        if (priority != DefaultInitPriority || initialise == 0)
        {
            return;
        }

        InitDecalPool(&g_DecalData);
        Platform::Graphics::InitParticleGraphics();
        // System 0: one particle a frame of a second's life, 500 big, grey, half alpha, on the first 256 pixels square
        constexpr u16 NullMaxParticles = 100;
        constexpr f32 NullUnusedFloat1 = 40000.0f;
        constexpr f32 NullGrey = 64.0f;
        constexpr f32 NullAlpha = 64.0f;
        constexpr f32 NullSize = 500.0f;
        constexpr f32 NullRotationRange = 360.0f;
        constexpr f32 NullTextureEnd = 256.0f;
        ParticleSystem& none = g_ParticleSystems[0];
        RetailLibc::MemorySet(&none, 0, sizeof(none));
        constexpr char NullName[] = "null";
        for (u32 letter = 0; letter < sizeof(NullName); letter++)
        {
            none.name[letter] = NullName[letter];
        }

        RetailLibc::MemorySet(none.name + sizeof(NullName), 0, sizeof(none.name) - sizeof(NullName));
        none.maxParticles = NullMaxParticles;
        none.onTime = 1;
        none.unusedFloat1 = NullUnusedFloat1;
        none.cutOffRadius = DefaultCutOffRadius;
        none.unusedFloat6 = DefaultUnusedFloat6;
        none.alphaKeys[0].value = NullAlpha;
        none.distortionY = DefaultDistortion;
        none.heightKeys[1].value = NullSize;
        none.minRotation = -NullRotationRange;
        none.maxRotation = NullRotationRange;
        none.unusedKeys2[1].time = 1.0f;
        none.textureEndY = NullTextureEnd;
        none.genRate = 1;
        none.velocity = 1.0f;
        none.lifeTime = 1.0f;
        none.colourKeys[0].red = NullGrey;
        none.colourKeys[0].green = NullGrey;
        none.colourKeys[0].blue = NullGrey;
        none.colourKeys[1].time = 1.0f;
        none.alphaKeys[1].time = 1.0f;
        none.distortionX = DefaultDistortion;
        none.maxSize = NullSize;
        none.widthKeys[0].value = NullSize;
        none.widthKeys[1].time = 1.0f;
        none.widthKeys[1].value = NullSize;
        none.heightKeys[0].value = NullSize;
        none.heightKeys[1].time = 1.0f;
        none.rotationKeys[1].time = 1.0f;
        none.unusedKeys1[1].time = 1.0f;
        none.textureEndX = NullTextureEnd;
        for (s16& slot : none.emitterSlots)
        {
            slot = -1;
        }
    }

    void KillParticles()
    {
        constexpr f32 TimeMoved = 10.0f;
        for (s32 index = 0; index < static_cast<s32>(MaxEmitterRuntimes); index++)
        {
            EmitterRuntime& runtime = g_EmitterRuntimes[index];
            if (runtime.system == 0 || runtime.state >= EmitterRuntime::StateKeptFrom)
            {
                continue;
            }

            ParticleSystem* system = g_LoadedParticleSystems[runtime.system];
            s32 released = index;
            ReleaseParticleEmitter(&released);
            ForgetParticleSystemDraws(system);
        }

        g_ParticleTime = g_ParticleTime + TimeMoved;
    }

    void UnloadAllParticleSections()
    {
        for (s32 slot = 0; slot < static_cast<s32>(ParticleSectionSlots); slot++)
        {
            if (g_ParticleSlotsUsed[slot] != 0)
            {
                UnloadParticleSection(static_cast<s8>(slot));
            }
        }
    }

    void ResetParticleEmitters()
    {
        for (s32 index = MaxParticleEmitters - 1; index >= 0; index--)
        {
            g_ParticleEmitters[index].runtime = -1;
        }

        for (u32 slot = 0; slot < ParticleSectionSlots; slot++)
        {
            g_ParticleSlotsUsed[slot] = 0;
            g_ParticleSlotsMade[slot] = 0;
        }

        g_ParticleEmitterCount = 0;
    }

    void KeepEmitterTranslation(s32 runtimeIndex)
    {
        EmitterRuntime& runtime = g_EmitterRuntimes[runtimeIndex];
        runtime.keepsEmitTranslation = 1;
        runtime.emitMatrix.m[3][2] = runtime.position[2];
        runtime.emitMatrix.m[3][0] = runtime.position[0];
        runtime.emitMatrix.m[3][1] = runtime.position[1];
    }

    void BuildParticleRenderTable(ParticleSystem* system)
    {
        constexpr f32 TextureSixteenths = 16.0f;
        constexpr f32 StepShare = 1.0f / Platform::Graphics::ParticleRenderSteps;
        constexpr f32 Half = 0.5f;
        constexpr f32 HexagonReach = 0.25f;
        // The hexagons' six corners, a sixth of a turn apart
        constexpr u32 HexagonCorners = 6;
        constexpr s32 HexagonAngles[HexagonCorners] = {0, 0x2AAA, 0x5555, 0x8000, 0xAAAA, 0xD555};
        if (system->renderTable == nullptr)
        {
            system->renderTable = g_RenderTablePool[g_RenderTablePoolTop];
            g_RenderTablePoolTop++;
        }

        Platform::Graphics::ParticleLook look;
        look.gravity = system->gravity;
        look.texture[0] = static_cast<s32>(system->textureStartX * TextureSixteenths);
        look.texture[2] = static_cast<s32>(system->textureEndX * TextureSixteenths);
        look.texture[1] = static_cast<s32>(system->textureStartY * TextureSixteenths);
        look.texture[3] = static_cast<s32>(system->textureEndY * TextureSixteenths);
        for (u32 step = 0; step < Platform::Graphics::ParticleRenderSteps; step++)
        {
            f32 life = static_cast<f32>(static_cast<s32>(step)) * StepShare;
            f32 width = SampleRenderCurve(system->widthKeys, life);
            f32 height = SampleRenderCurve(system->heightKeys, life);
            f32 rotation = SampleRenderCurve(system->rotationKeys, life);
            f32 turn[4];
            SinCos16Pair(static_cast<s32>(rotation), static_cast<s32>(rotation), turn);
            f32 sine = turn[0];
            f32 cosine = turn[1];
            // The jibber: the frequencies' cycles over the life
            f32 jibber[4];
            SinCos16Pair(static_cast<s32>(system->jibberXFreq * life * UnitsPerTurn), static_cast<s32>(system->jibberYFreq * life * UnitsPerTurn),
                         jibber);
            f32 jibberX = system->jibberXAmp * jibber[0];
            f32 jibberY = system->jibberYAmp * jibber[2];
            f32 aspect = static_cast<f32>(g_DisplayWidth) / static_cast<f32>(g_DisplayHeight);
            f32* shape = look.steps[step].shape;
            f32 cornerX = -(width * Half) * cosine - height * (sine * Half) + jibberX;
            f32 cornerY = width * (sine * Half) - height * (cosine * Half) + jibberY;
            f32 otherX = width * (cosine * Half) - height * (sine * Half) + jibberX;
            shape[0] = cornerX * (aspect + aspect);
            shape[1] = cornerY + cornerY;
            f32 otherY = -(width * Half) * sine - height * (cosine * Half) + jibberY;
            f32 rise = height * sine;
            shape[4] = otherX * (aspect + aspect);
            shape[5] = otherY + otherY;
            f32 upY = height * cosine + height * cosine;
            f32 upX = rise * aspect + rise * aspect;
            shape[5] = shape[5] + upY;
            shape[3] = upY;
            shape[2] = upX;
            shape[4] = shape[4] + upX;
            f32 colour[3];
            SampleRenderColour(system->colourKeys, life, colour);
            f32 alpha = SampleRenderCurve(system->alphaKeys, life);
            look.steps[step].colour[0] = static_cast<u8>(static_cast<s32>(colour[0]));
            look.steps[step].colour[1] = static_cast<u8>(static_cast<s32>(colour[1]));
            look.steps[step].colour[2] = static_cast<u8>(static_cast<s32>(colour[2]));
            look.steps[step].colour[3] = static_cast<u8>(static_cast<s32>(alpha));
        }

        look.hexagons = DrawsHexagons(system);
        if (look.hexagons)
        {
            for (u32 corner = 0; corner < HexagonCorners; corner += 2)
            {
                f32 sines[4];
                SinCos16Pair(HexagonAngles[corner], HexagonAngles[corner + 1], sines);
                look.hexagonCorners[corner * 2] = sines[1] * HexagonReach;
                look.hexagonCorners[corner * 2 + 1] = sines[0] * HexagonReach;
                look.hexagonCorners[corner * 2 + 2] = sines[3] * HexagonReach;
                look.hexagonCorners[corner * 2 + 3] = sines[2] * HexagonReach;
            }
        }

        Platform::Graphics::WriteParticleRenderTable(system->renderTable, look);
    }

    void GenCode1_VelocityXZ2xStart(EmitterRuntime*, ParticleSystem*, ParticleRecord* record)
    {
        record->velocity[0] = record->start[0] + record->start[0];
        record->velocity[2] = record->start[2] + record->start[2];
    }

    void GenCode2_VelocityXZMinusStart(EmitterRuntime*, ParticleSystem*, ParticleRecord* record)
    {
        record->velocity[0] = -record->start[0];
        record->velocity[2] = -record->start[2];
    }

    void GenCode3_VelocityXZ4xStart(EmitterRuntime*, ParticleSystem*, ParticleRecord* record)
    {
        record->velocity[0] = record->start[0] * 4.0f;
        record->velocity[2] = record->start[2] * 4.0f;
    }

    void GenCode4_VelocityXZ16xStart(EmitterRuntime*, ParticleSystem*, ParticleRecord* record)
    {
        record->velocity[0] = record->start[0] * 16.0f;
        record->velocity[2] = record->start[2] * 16.0f;
    }

    void GenCode5_PullInRandomLife(EmitterRuntime*, ParticleSystem* system, ParticleRecord* record)
    {
        constexpr f32 PullIn = -0x1.333334p-1f;
        constexpr f32 PullDown = 0x1.99999Ap-2f;
        f32 x = record->start[0];
        f32 z = record->start[2];
        f32 flat = __builtin_sqrtf(x * x + z * z);
        record->velocity[2] = record->velocity[2] + z * PullIn;
        record->velocity[0] = record->velocity[0] + x * PullIn;
        record->start[1] = record->start[1] - flat * PullDown;
        record->lifeScale = RandomLifeScale(system);
    }

    void GenCode6_Velocity5_4xStart(EmitterRuntime*, ParticleSystem*, ParticleRecord* record)
    {
        constexpr f32 Times = 0x1.59999Ap+2f;
        record->velocity[0] = record->start[0] * Times;
        record->velocity[1] = record->start[1] * Times;
        record->velocity[2] = record->start[2] * Times;
    }

    s32 GenerateParticles(ParticleViews* views)
    {
        s32 generating = 0;
        EmitterRuntime* runtime = g_ParticleEmitterWheel[g_ParticleEmitterWheelIndex];
        while (runtime != nullptr)
        {
            EmitterRuntime* next = runtime->next;
            ParticleSystem* system = g_LoadedParticleSystems[runtime->system];
            if (runtime->phase != 0)
            {
                runtime = next;
                continue;
            }

            s32 view = ParticleViewOf(views, runtime->chunk);
            runtime->cameraDistance = view == -1 ? NoViewDistance : Platform::Math::ViewDistance(view, runtime->position);
            if (runtime->enabled != 0)
            {
                if (runtime->cameraDistance < system->cutOnRadius ||
                    (0.0f < system->cutOffRadius && system->cutOffRadius < runtime->cameraDistance))
                {
                    runtime->enabled = 0;
                }
            }

            if (static_cast<s8>(runtime->cameraSwitch) == EmitterRuntime::CameraNeverOn)
            {
                runtime->enabled = 0;
            }

            if (static_cast<s8>(runtime->cameraSwitch) == EmitterRuntime::CameraAlwaysOn)
            {
                runtime->enabled = 1;
            }

            if (runtime->enabled == 0)
            {
                // Back on within the radiuses (no cut off radius: any distance past the cut on), not before the next time it's run
                if (system->cutOnRadius <= runtime->cameraDistance &&
                    (system->cutOffRadius == 0.0f || runtime->cameraDistance <= system->cutOffRadius))
                {
                    runtime->enabled = 1;
                }

                if (runtime->enabled == 0 && runtime->blocksWanted != 0)
                {
                    // Its blocks let go of
                    runtime->maxParticles = 0;
                    runtime->blocksWanted = 0;
                    if (runtime->blocksWanted == runtime->blockCount)
                    {
                        runtime->capacity = runtime->maxParticles;
                    }
                    else
                    {
                        AllocParticleEmitterBlocks(runtime);
                    }
                }

                runtime = next;
                continue;
            }

            if (runtime->blocksWanted == 0)
            {
                // The blocks its system's particles take
                const ParticleSystem* current = g_LoadedParticleSystems[runtime->system];
                s32 particles = static_cast<s16>(system->maxParticles);
                runtime->maxParticles = static_cast<s16>(particles);
                bool hexagons = DrawsHexagons(current);
                runtime->blocksWanted = static_cast<s16>(hexagons ? (particles + HexagonBlockParticles - 1) / HexagonBlockParticles
                                                                  : (particles + BlockParticles - 1) / BlockParticles);
                if (runtime->blocksWanted > MaxEmitterBlocks)
                {
                    runtime->blocksWanted = MaxEmitterBlocks;
                    runtime->maxParticles = DrawsHexagons(current) ? MaxHexagonParticles : MaxBlockParticles;
                }

                if (runtime->blocksWanted == runtime->blockCount)
                {
                    runtime->capacity = runtime->maxParticles;
                }
                else
                {
                    AllocParticleEmitterBlocks(runtime);
                }
            }

            if (runtime->blockCount <= 0)
            {
                runtime = next;
                continue;
            }

            if (system->genRate > 0)
            {
                generating++;
                for (s32 particle = 0; particle < system->genRate; particle++)
                {
                    runtime->generator(runtime, system);
                }
            }
            else if (system->genRate < 0 && g_ParticleFrameCounter % -system->genRate == 0)
            {
                generating++;
                runtime->generator(runtime, system);
            }

            runtime = next;
        }

        return generating;
    }

    void UpdateAndDrawParticles(s32 frozen, f32 delta)
    {
        g_CollidingCount = 0;
        Platform::Graphics::UseHelperPrograms(Platform::Graphics::CullingPrograms, false);
        s32 active = 1;
        ParticleViews views;
        views.frame = g_RenderedFrames;
        g_ParticleFrameDelta = delta;
        views.count = 0;
        if (g_ParticlesSetUp != 0)
        {
            if (frozen == 0)
            {
                if (g_ParticlesOn == 0)
                {
                    active = 0;
                }
                else
                {
                    g_ParticleTime = g_ParticleTime + delta;
                    g_ParticleFrameCounter++;
                    UpdateParticleCollisionSpheres();
                    EmitterRuntime* runtime = g_ParticleEmitterWheel[g_ParticleEmitterWheelIndex];
                    while (runtime != nullptr)
                    {
                        EmitterRuntime* next = runtime->next;
                        if (runtime->maxParticles != runtime->capacity)
                        {
                            AllocParticleEmitterBlocks(runtime);
                        }

                        runtime = next;
                    }

                    active = GenerateParticles(&views);
                    UpdateParticleEmitterTiming();
                    RescheduleParticleEmitters();
                    ProcessParticleBlockEvents();
                }
            }

            active += DrawParticleList(frozen, 0, &views);
            active += DrawParticleList(frozen, DrawListDistortion, &views);
        }

        if (active == 0)
        {
            g_ParticleTime = 0.0f;
        }

        for (s32 index = 0; index < views.count; index++)
        {
            views.chunks[index]->particleView = -1;
        }
    }
}

namespace
{
// A page's startup file, and the extension of the path made for each page of the default chunk's
constexpr const char* PageFile = "startup\\%s%d.ptc";
constexpr const char* PageExtension = ".ptl";
// The default particle data's decals' startup file
constexpr const char* DecalFile = "Startup\\decal.ptc";
// The blocks of particles made at start-up
constexpr s32 StartupBlocks = 0x80;
}

extern "C"
{
    Material* ParticlePageMaterial(u32 page)
    {
        return g_ParticlePages[page].materials[BlendAdditive];
    }

    void ReadParticlePageAt(u32 page, Stream* stream)
    {
        Platform::Graphics::ReadParticlePage(&g_ParticlePages[page], stream, false);
    }

    void LoadParticlePages(const char* name, s32 blocks)
    {
        for (u32 page = 0; page < ParticlePageCount; page++)
        {
            char path[0x100];
            RetailLibc::Format(path, PageFile, name, page);
            Platform::Graphics::LoadParticlePage(&g_ParticlePages[page], path, false);
        }

        InitParticleSystemsForLevel(blocks);
    }

    s32 LoadParticles(const char* name)
    {
        LoadParticlePages(name, StartupBlocks);
        LoadDecals(&g_DecalData, DecalFile);
        return 1;
    }

    s32 SetUpDefaultParticles()
    {
        InitParticleSystemsForLevel(StartupBlocks);
        SetDefaultDecalTypes(&g_DecalData);
        return 1;
    }

    void ReadParticleData(Rm2Reader* reader, Stream* stream)
    {
        String path;
        path.string = nullptr;
        path.length = 0;
        path.capacity = 0;
        if (reader->bits.defaultChunk != 0)
        {
            UnloadAllParticleSections();
            for (u32 page = 0; page < ParticlePageCount; page++)
            {
                StringAssign(&path, reader->path.string);
                String number;
                StringConstructNumber(&number, page);
                StringAppend(&path, number.string);
                StringDestroy(&number);
                StringAppend(&path, PageExtension);
                ReadParticlePageAt(page, stream);
            }

            SetParticleReader(stream);
            ReadParticleSection(SectionDefault, nullptr);
            ReadDecalPage(&g_DecalData, stream);
            ReadDecalData(&g_DecalData, stream);
        }
        else
        {
            ChunkDataReference* reference = reader->entry->data;
            ChunkData* chunk = reference != nullptr ? reference->chunk : nullptr;
            StringAssign(&path, reader->path.string);
            StringAppend(&path, PageExtension);
            SetParticleReader(stream);
            ReadParticleSection(SectionLevel, chunk);
        }

        StringDestroy(&path);
    }

    void ParticlesStaticInit()
    {
        InitParticles(1, DefaultInitPriority);
    }
}

EABI_EXPORT(CreateParticleEmitter, CreateParticleEmitter);
EABI_EXPORT(TestParticleCollisionSpheres, TestParticleCollisionSpheres);
EABI_EXPORT(FUN_001ba148, TestParticleCollision);
EABI_EXPORT(UpdateAndDrawParticles, UpdateAndDrawParticles);
