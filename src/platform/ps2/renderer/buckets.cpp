#include "renderer.h"

#include "game/context.h"
#include "game/particles.h"

#include "platform/graphics.h"

#include <ee_regs.h>
#include <kernel.h>

namespace
{
// Each region holds a buffer of every chain
constexpr u32 RegionSize = 0x187040;
constexpr s32 ChainLimit = 11;
// The frame's chains, and the buckets of each from these on
constexpr u32 FrameChainCapacity = 0x7530;
constexpr u32 FrameChainKind = 1;
constexpr u32 FrameChainFirstBuckets[] = {0, 5, 21};
constexpr u32 LargeChainCapacity = 0x2710;
constexpr u32 LargeChainKind = 2;
constexpr u32 SmallChainCapacity = 0x64;
constexpr u32 SmallChainKind = 8;

constexpr u32 Vif1Channel = 1;
// D_CHCR: from memory, chain mode, the tags' second halves sent to the VIF, started
constexpr u32 ChainWithTags = 0x145;

// A chain of capacity quadwords in both regions, and movie buffers a tenth of its size. Returns its index, -1 when there's no
// room
s32 AllocateChain(u32 capacity, u32 kind)
{
    u32 allocated = g_DmaQuadwordsAllocated;
    if ((allocated + capacity) * 16 > RegionSize || g_DmaChainCount >= ChainLimit)
    {
        return -1;
    }

    s32 index = g_DmaChainCount;
    DmaChain& chain = g_DmaChains[index];
    u32 offset = allocated * 16;
    u32 movieSize = capacity * 16 / 10;
    chain.buffers[0] = g_DmaFirstBuffers + offset;
    chain.buffers[1] = g_DmaSecondBuffers + offset;
    chain.movieBuffers[0] = g_DmaMovieNext;
    chain.movieBuffers[1] = g_DmaMovieNext + movieSize;
    g_DmaMovieNext += 2 * movieSize;
    chain.next = chain.buffers[0];
    chain.start = chain.buffers[0];
    chain.capacity = capacity;
    chain.buffer = 0;
    chain.kind = kind;
    chain.unknown24 = 0;
    g_DmaQuadwordsAllocated = allocated + capacity;
    g_DmaChainCount = index + 1;
    return index;
}

// The other buffer (or movie buffer) becomes the one filled
void SwapBuffers(DmaChain& chain)
{
    chain.buffer ^= 1;
    u8* const* buffers = g_MovieBuckets != 0 ? chain.movieBuffers : chain.buffers;
    chain.next = buffers[chain.buffer];
    chain.start = buffers[chain.buffer];
    chain.unknown24 = 0;
}

// The bucket starts with a "next" tag sending a quadword of VIF1's FLUSHA. Its address is its own until the bucket is linked
void StartBucket(RenderBucket& bucket)
{
    DmaChain& chain = ChainOf(bucket);
    auto* tag = reinterpret_cast<u32*>(chain.next);
    bucket.first = tag;
    bucket.insertion = tag;
    bucket.last = tag;
    tag[0] = NextTag | 1;
    tag[1] = Address(bucket.last);
    tag[2] = 0;
    tag[3] = 0;
    tag[4] = 0;
    tag[5] = 0;
    tag[6] = 0;
    tag[7] = VifFlushA;
    chain.next = reinterpret_cast<u8*>(tag + 8);
    bucket.unknown34 = 0;
    bucket.lastKey = 0;
    bucket.last2DMaterial = nullptr;
    bucket.unknown28 = 0;
    bucket.lastJoints = 0;
    bucket.lastCall = 0;
}

void StartFrameBuckets(FrameBuckets* buckets)
{
    for (u32 first : FrameChainFirstBuckets)
    {
        SwapBuffers(ChainOf(buckets->buckets[first]));
    }

    for (RenderBucket& bucket : buckets->buckets)
    {
        StartBucket(bucket);
    }

    PutVUProgramsIntoDMAPipeline();
    FUN_001bc650();
}

// A writer of its own: its first tags are "next" tags of no quadwords, each to the one after, the last one to itself
void StartWriterTags(RenderBucket* writer, u32 bucket, u32 tags)
{
    writer->chain = g_FrameBuckets.buckets[bucket].chain;
    DmaChain& chain = ChainOf(*writer);
    auto* tag = reinterpret_cast<u32*>(chain.next);
    writer->first = tag;
    writer->insertion = tag;
    writer->last = tag + (tags - 1) * 4;
    for (u32 i = 0; i < tags; i++)
    {
        u32* at = tag + i * 4;
        at[0] = NextTag;
        at[1] = Address(i + 1 < tags ? at + 4 : writer->last);
        at[2] = 0;
        at[3] = 0;
    }

    chain.next = reinterpret_cast<u8*>(tag + tags * 4);
    writer->unknown34 = 0;
    writer->lastKey = 0;
    writer->last2DMaterial = nullptr;
    writer->unknown28 = 0;
    writer->lastCall = 0;
}
}

extern "C"
{
    u8* CarveDmaMemory(u8* memory)
    {
        u8* first = reinterpret_cast<u8*>((Address(memory) + 0xF) & ~0xFu);
        g_DmaFirstBuffers = first;
        u8* second = reinterpret_cast<u8*>((Address(first + RegionSize) + 0xF) & ~0xFu);
        g_DmaQuadwordsAllocated = 0;
        g_DmaSecondBuffers = second;
        g_DmaMovieNext = g_DmaFirstBuffers;
        g_DmaChainCount = 0;
        SetDmaRegisterPointers();
        g_DmaReady = 1;
        return second + RegionSize;
    }

    void InitialiseFrameBuckets(FrameBuckets* buckets)
    {
        s32 chains[3];
        for (s32& chain : chains)
        {
            chain = AllocateChain(FrameChainCapacity, FrameChainKind);
        }

        for (u32 i = 0; i < 28; i++)
        {
            u32 chain = i < FrameChainFirstBuckets[1] ? 0 : i < FrameChainFirstBuckets[2] ? 1 : 2;
            buckets->buckets[i].chain = chains[chain];
            StartBucket(buckets->buckets[i]);
        }

        PutVUProgramsIntoDMAPipeline();
        FUN_001bc650();
    }

    void InitialiseSmallBucket(SingleBucket* bucket)
    {
        bucket->buckets[0].chain = AllocateChain(SmallChainCapacity, SmallChainKind);
        StartBucket(bucket->buckets[0]);
    }

    void InitialiseLargeBucket(SingleBucket* bucket)
    {
        bucket->buckets[0].chain = AllocateChain(LargeChainCapacity, LargeChainKind);
        StartBucket(bucket->buckets[0]);
    }

    void StartWriter(RenderBucket* writer, u32 bucket)
    {
        StartWriterTags(writer, bucket, 1);
    }

    void StartWriterTwoTags(RenderBucket* writer, u32 bucket)
    {
        StartWriterTags(writer, bucket, 2);
    }

    u8* AllocDmaTags(FrameBuckets* buckets, u32 count, u32 size)
    {
        DmaChain& chain = ChainOf(buckets->buckets[0]);
        u8* start = chain.next;
        chain.next = start + ((count * size) >> 4 << 4) + 0x10;
        return start;
    }

    void ResetRenderBuckets(FrameBuckets* buckets)
    {
        StartFrameBuckets(buckets);
    }

    void LinkRenderBuckets(FrameBuckets* buckets, bool fromInterrupt)
    {
        RenderBucket* bucket = buckets->buckets;
        for (u32 i = 0; i + 1 < 28; i++)
        {
            bucket[i].last[1] = Address(bucket[i + 1].first);
        }

        // The frame ends with an END tag in the last bucket's chain
        RenderBucket& last = bucket[27];
        DmaChain& lastChain = ChainOf(last);
        auto* end = reinterpret_cast<u32*>(lastChain.next);
        last.last[1] = Address(end);
        end[0] = EndTag;
        end[1] = 0;
        end[2] = VifFlushA;
        end[3] = 0;
        lastChain.next = reinterpret_cast<u8*>(end + 4);

        // VIF1's channel sends it from the first bucket's chain's start
        DmaChain& first = ChainOf(bucket[0]);
        if (fromInterrupt)
        {
            iFlushCache(0);
        }
        else
        {
            FlushCache(0);
        }

        RendererDmaChannel& vif1 = g_RendererDma[Vif1Channel];
        *R_EE_D_STAT = vif1.statusBit;
        vif1.sending = 1;
        *R_EE_D1_QWC = 0;
        u8* const* buffers = g_MovieBuckets != 0 ? first.movieBuffers : first.buffers;
        *R_EE_D1_TADR = Address(buffers[first.buffer]) & 0x0FFFFFFF;
        *R_EE_D1_CHCR = ChainWithTags;
        asm volatile("sync.l\n\tsync.p" ::: "memory");

        StartFrameBuckets(buckets);
    }
}

extern "C" void RenderBucketConstruct(RenderBucket* bucket)
{
    bucket->unknown34 = 0;
    bucket->first = nullptr;
    bucket->last = nullptr;
    bucket->insertion = nullptr;
    bucket->chain = 0;
    bucket->lastKey = 0;
    bucket->vuBuffer = 0;
    bucket->unknown0D = 0;
    bucket->programRegion = 0;
    bucket->last2DMaterial = nullptr;
    bucket->unknown28 = 0;
    bucket->lastJoints = 0;
    bucket->lastCall = 0;
}

extern "C"
{
    // The renderer's statics made (GCC 2.9x's static initialisation: no particle section's file, the screen's places on the
    // two TVs none, the frame's buckets and two of their own emptied, the models' update rate), and the static constructor that
    // runs it
    void InitRendererStatics(s32 initialise, s32 priority) RETAIL(FUN_0019fee0);
    void RendererStaticInit() RETAIL(FUN_001a6678);
}

void InitRendererStatics(s32 initialise, s32 priority)
{
    constexpr s32 AllPriorities = 0xFFFF;
    if (priority != AllPriorities || initialise == 0)
    {
        return;
    }

    g_ParticleSectionFile = -1;
    g_PalScreenOffset.x = 0.0f;
    g_PalScreenOffset.y = 0.0f;
    g_NtscScreenOffset.x = 0.0f;
    g_NtscScreenOffset.y = 0.0f;
    RenderBucketConstruct(&g_SmallBucket.buckets[0]);
    for (RenderBucket& bucket : g_FrameBuckets.buckets)
    {
        RenderBucketConstruct(&bucket);
    }

    RenderBucketConstruct(&g_LargeBucket.buckets[0]);
    g_ModelUpdateRate.cutoff = 0xFFFF;
    g_ModelUpdateRate.slope = 1.0f;
    g_ModelUpdateRate.grace = 0;
}

void RendererStaticInit()
{
    InitRendererStatics(1, 0xFFFF);
}

void Platform::Graphics::ResetBuckets(bool movie)
{
    g_MovieBuckets = movie;
    ResetRenderBuckets(&g_FrameBuckets);
}

void Platform::Graphics::SubmitBuckets(bool fromInterrupt)
{
    LinkRenderBuckets(&g_FrameBuckets, fromInterrupt);
}

void* Platform::Graphics::AllocFrameMemory(u32 count, u32 size)
{
    return AllocDmaTags(&g_FrameBuckets, count, size);
}
