#include "common.h"
#include "retail/libc.h"

#include <kernel.h>

// The C library's heap, Sony's newlib's: Doug Lea's malloc 2.6.4 aligned to 16 bytes, its sizes 32 bits and its longs 64 (the
// remainders, the trim threshold, top_pad and two of the statistics). The pools come from it (Platform::Memory::AllocatePool),
// and newlib's sprintf takes _malloc_r, _free_r and _realloc_r from here, so there's one heap and one break
namespace
{
// A chunk: its size has bit 0 set while the chunk before it is in use; a free one is in a bin's list
struct Chunk
{
    u32 previousSize;
    u32 size;
    Chunk* next;
    Chunk* previous;
};

constexpr u32 PreviousInUse = 0x1;
constexpr u32 SizeBits = ~0x3u;
constexpr u32 MinimumSize = 0x10;
constexpr u32 AlignMask = 0xF;
constexpr u32 PageSize = 0x1000;
// A chunk's memory comes after its two size fields; a chunk in use takes the next one's previous size as its own (SIZE_SZ)
constexpr u32 ChunkHeaderSize = offsetof(Chunk, next);
constexpr u32 SizeFieldSize = sizeof(u32);
// The small bins are 8 bytes apart (a bin index is the size / 8), below SmallBinLimit
constexpr u32 SmallBinShift = 3;
constexpr u32 SmallBinLimit = 0x200;
// Requests below it look for their own size in the small bins first
constexpr u32 SmallRequestLimit = SmallBinLimit - (1u << SmallBinShift);
constexpr s32 LastSmallBin = 0x3F;
// The bins' list heads are a next and a previous pointer each, a bin's chunk starting 8 bytes before its head
constexpr u32 BinSize = 2 * sizeof(Chunk*);
constexpr s32 BinBlockWidth = 4;
// ENOMEM
constexpr s32 OutOfMemory = 12;
}

extern "C"
{
    // __malloc_av_: 128 bins, bin i the chunk 8 * i bytes in whose next and previous are the bin's list (the array's first
    // word is no chunk's): bin 0's next is the top chunk, its size the bin blocks' bits (a block of 4 bins may have chunks),
    // bin 1 the last remainder of a split
    extern u8 g_MallocBins[] RETAIL(D_002EA410);
    extern u64 g_MallocTrimThreshold RETAIL(D_002EA818);
    extern u64 g_MallocTopPad RETAIL(D_002EA820);
    extern u8* g_MallocSbrkBase RETAIL(D_002EA828);
    extern u64 g_MallocMaxSbrked RETAIL(D_002EA830);
    extern u64 g_MallocMaxTotal RETAIL(D_002EA838);
    // current_mallinfo.arena: what the heap took from sbrk
    extern u32 g_MallocSbrked RETAIL(D_002EA840);
    // The global errno _sbrk_r takes sbrk's error from
    extern s32 g_Errno RETAIL(D_0030ACD4);
    // sbrk's break, the program's end to start with
    extern u8* g_HeapEnd RETAIL(D_002EABE4);
    // _impure_ptr: malloc and free work on the C library's reentrancy block (its first word is errno)
    extern s32* g_MallocReent RETAIL(D_002EA3CC);

    s32* ErrorNumber() RETAIL(FUN_002c7040);

    void* MallocReentrant(s32* reent, u32 size) RETAIL(FUN_002cc308);
    void FreeReentrant(s32* reent, void* memory) RETAIL(FUN_002cca38);
    // malloc_extend_top: more of the break for the top chunk
    void ExtendTop(s32* reent, u32 size) RETAIL(FUN_002cc0a0);
    // malloc_trim: gives the top chunk's memory past pad back to sbrk
    s32 Trim(s32* reent, u32 pad) RETAIL(FUN_002ccd30);
    void* SbrkReentrant(s32* reent, s32 increment) RETAIL(FUN_002cde28);
}

namespace
{
Chunk* BinAt(s32 index)
{
    return reinterpret_cast<Chunk*>(g_MallocBins + BinSize * index);
}

Chunk* LastRemainder()
{
    return BinAt(1);
}

Chunk* Top()
{
    return BinAt(0)->next;
}

// The chunk offset bytes on (or back, the offset wrapping round)
Chunk* At(Chunk* chunk, u32 offset)
{
    return reinterpret_cast<Chunk*>(reinterpret_cast<u32>(chunk) + offset);
}

void* MemoryOf(Chunk* chunk)
{
    return reinterpret_cast<u8*>(chunk) + ChunkHeaderSize;
}

Chunk* ChunkOf(void* memory)
{
    return reinterpret_cast<Chunk*>(static_cast<u8*>(memory) - ChunkHeaderSize);
}

u32 SizeOf(Chunk* chunk)
{
    return chunk->size & SizeBits;
}

// long_sub_size_t: the difference as a long
s64 Difference(u32 size, u32 wanted)
{
    return static_cast<s64>(size) - static_cast<s64>(wanted);
}

void SetInUse(Chunk* chunk, u32 size)
{
    At(chunk, size)->size |= PreviousInUse;
}

void Unlink(Chunk* chunk)
{
    Chunk* previous = chunk->previous;
    Chunk* next = chunk->next;
    next->previous = previous;
    previous->next = next;
}

void LinkLastRemainder(Chunk* chunk)
{
    Chunk* bin = LastRemainder();
    bin->previous = chunk;
    bin->next = chunk;
    chunk->next = bin;
    chunk->previous = bin;
}

void MarkBinBlock(s32 index)
{
    BinAt(0)->size |= static_cast<u32>(1ull << (index / BinBlockWidth));
}

// bin_index: the small bins 8 bytes apart, then bins 64, 512, 4096, 32768 and 262144 bytes apart, the last bin for the rest
s32 BinIndex(u32 size)
{
    u32 large = size >> 9;
    if (large == 0)
    {
        return static_cast<s32>(size >> SmallBinShift);
    }

    if (large <= 4)
    {
        return static_cast<s32>(size >> 6) + 56;
    }

    if (large <= 20)
    {
        return static_cast<s32>(large) + 91;
    }

    if (large <= 84)
    {
        return static_cast<s32>(size >> 12) + 110;
    }

    if (large <= 340)
    {
        return static_cast<s32>(size >> 15) + 119;
    }

    if (large <= 1364)
    {
        return static_cast<s32>(size >> 18) + 124;
    }

    return 126;
}

// frontlink: small chunks go first in their bin, large ones before the first smaller one (their bins are sorted)
void FrontLink(Chunk* chunk, u32 size)
{
    Chunk* bin;
    Chunk* next;
    if (size < SmallBinLimit)
    {
        s32 index = static_cast<s32>(size >> SmallBinShift);
        MarkBinBlock(index);
        bin = BinAt(index);
        next = bin->next;
    }
    else
    {
        s32 index = BinIndex(size);
        bin = BinAt(index);
        next = bin->next;
        if (next == bin)
        {
            MarkBinBlock(index);
        }
        else
        {
            while (next != bin && size < SizeOf(next))
            {
                next = next->next;
            }

            bin = next->previous;
        }
    }

    chunk->previous = bin;
    chunk->next = next;
    bin->next = chunk;
    next->previous = chunk;
}

// Splits chunk's first size bytes off, the rest becoming the last remainder
void* SplitOff(Chunk* chunk, u32 size, s64 remainderSize)
{
    Chunk* remainder = At(chunk, size);
    LinkLastRemainder(remainder);
    remainder->size = static_cast<u32>(remainderSize) | PreviousInUse;
    At(remainder, static_cast<u32>(remainderSize))->previousSize = static_cast<u32>(remainderSize);
    return MemoryOf(chunk);
}
}

// malloc_extend_top
void ExtendTop(s32* reent, u32 size)
{
    Chunk* oldTop = Top();
    u32 oldTopSize = SizeOf(oldTop);
    u8* oldEnd = reinterpret_cast<u8*>(At(oldTop, oldTopSize));
    u32 sbrkSize = static_cast<u32>(size + g_MallocTopPad + MinimumSize);
    if (g_MallocSbrkBase != reinterpret_cast<u8*>(-1))
    {
        sbrkSize = (sbrkSize + (PageSize - 1)) & ~(PageSize - 1);
    }

    u8* end = static_cast<u8*>(SbrkReentrant(reent, static_cast<s32>(sbrkSize)));
    if (end == reinterpret_cast<u8*>(-1) || (end < oldEnd && oldTop != BinAt(0)))
    {
        return;
    }

    g_MallocSbrked += sbrkSize;
    if (end == oldEnd)
    {
        Top()->size = (sbrkSize + oldTopSize) | PreviousInUse;
    }
    else
    {
        if (g_MallocSbrkBase == reinterpret_cast<u8*>(-1))
        {
            g_MallocSbrkBase = end;
        }
        else
        {
            g_MallocSbrked += end - oldEnd;
        }

        u32 misalignment = reinterpret_cast<u32>(end + ChunkHeaderSize) & AlignMask;
        u32 correction = 0;
        if (misalignment != 0)
        {
            correction = AlignMask + 1 - misalignment;
            end += correction;
        }

        // The next break on a page boundary: a whole page more when it's on one already
        correction += PageSize - (reinterpret_cast<u32>(end + sbrkSize) & (PageSize - 1));
        u8* newEnd = static_cast<u8*>(SbrkReentrant(reent, static_cast<s32>(correction)));
        if (newEnd == reinterpret_cast<u8*>(-1))
        {
            return;
        }

        g_MallocSbrked += correction;
        Chunk* top = reinterpret_cast<Chunk*>(end);
        BinAt(0)->next = top;
        top->size = (newEnd - end + correction) | PreviousInUse;
        if (oldTop != BinAt(0))
        {
            // Someone else moved the break: the old top becomes a free chunk behind two fenceposts
            if (oldTopSize < MinimumSize)
            {
                Top()->size = PreviousInUse;
                return;
            }

            oldTopSize = (oldTopSize - 3 * SizeFieldSize) & ~AlignMask;
            oldTop->size = (oldTop->size & PreviousInUse) | oldTopSize;
            At(oldTop, oldTopSize)->size = SizeFieldSize | PreviousInUse;
            At(oldTop, oldTopSize + SizeFieldSize)->size = SizeFieldSize | PreviousInUse;
            if (oldTopSize >= MinimumSize)
            {
                FreeReentrant(reent, MemoryOf(oldTop));
            }
        }
    }

    s64 sbrked = static_cast<s32>(g_MallocSbrked);
    if (g_MallocMaxSbrked < static_cast<u64>(sbrked))
    {
        g_MallocMaxSbrked = sbrked;
    }

    sbrked = static_cast<s32>(g_MallocSbrked);
    if (g_MallocMaxTotal < static_cast<u64>(sbrked))
    {
        g_MallocMaxTotal = sbrked;
    }
}

// _malloc_r: the exact or best fitting chunk of the bins, the last remainder, or a piece of the top chunk
void* MallocReentrant(s32* reent, u32 size)
{
    u32 wanted = size + SizeFieldSize + AlignMask < MinimumSize + AlignMask ? MinimumSize
                                                                            : (size + SizeFieldSize + AlignMask) & ~AlignMask;
    s32 index;
    if (wanted < SmallRequestLimit)
    {
        index = static_cast<s32>(wanted >> SmallBinShift);
        Chunk* bin = BinAt(index);
        Chunk* victim = bin->previous;
        if (victim != bin)
        {
            u32 victimSize = SizeOf(victim);
            Unlink(victim);
            SetInUse(victim, victimSize);
            return MemoryOf(victim);
        }

        // Its bin was looked in, the next is an empty one (the sizes are 16 apart)
        index += 2;
    }
    else
    {
        index = BinIndex(wanted);
        Chunk* bin = BinAt(index);
        for (Chunk* victim = bin->previous; victim != bin; victim = victim->previous)
        {
            u32 victimSize = SizeOf(victim);
            s64 remainderSize = Difference(victimSize, wanted);
            if (remainderSize >= MinimumSize)
            {
                index--;
                break;
            }

            if (remainderSize >= 0)
            {
                Unlink(victim);
                SetInUse(victim, victimSize);
                return MemoryOf(victim);
            }
        }

        index++;
    }

    Chunk* victim = LastRemainder()->next;
    if (victim != LastRemainder())
    {
        u32 victimSize = SizeOf(victim);
        s64 remainderSize = Difference(victimSize, wanted);
        if (remainderSize >= MinimumSize)
        {
            victim->size = wanted | PreviousInUse;
            return SplitOff(victim, wanted, remainderSize);
        }

        LastRemainder()->previous = LastRemainder();
        LastRemainder()->next = LastRemainder();
        if (remainderSize >= 0)
        {
            SetInUse(victim, victimSize);
            return MemoryOf(victim);
        }

        FrontLink(victim, victimSize);
    }

    // The bins of the blocks that may have chunks, from the request's own on: the first chunk big enough
    u64 block = 1ull << (index / BinBlockWidth);
    if (block <= BinAt(0)->size)
    {
        if ((block & BinAt(0)->size) == 0)
        {
            index = (index & ~(BinBlockWidth - 1)) + BinBlockWidth;
            block <<= 1;
            while ((block & BinAt(0)->size) == 0)
            {
                index += BinBlockWidth;
                block <<= 1;
            }
        }

        for (;;)
        {
            s32 start = index;
            Chunk* first = BinAt(index);
            Chunk* bin = first;
            do
            {
                for (victim = bin->previous; victim != bin; victim = victim->previous)
                {
                    u32 victimSize = SizeOf(victim);
                    s64 remainderSize = Difference(victimSize, wanted);
                    if (remainderSize >= MinimumSize)
                    {
                        victim->size = wanted | PreviousInUse;
                        Unlink(victim);
                        return SplitOff(victim, wanted, remainderSize);
                    }

                    if (remainderSize >= 0)
                    {
                        SetInUse(victim, victimSize);
                        Unlink(victim);
                        return MemoryOf(victim);
                    }
                }

                bin = At(bin, BinSize);
                // The small bins' sizes are 16 apart: every other one stays empty
                if (index < LastSmallBin)
                {
                    bin = At(bin, BinSize);
                    index++;
                }
            } while ((++index & (BinBlockWidth - 1)) != 0);

            // The block's bit goes when its bins are all empty (the ones before the start of a partly searched block too)
            do
            {
                if ((start & (BinBlockWidth - 1)) == 0)
                {
                    BinAt(0)->size &= ~static_cast<u32>(block);
                    break;
                }

                start--;
                first = At(first, -BinSize);
            } while (first->next == first);

            block <<= 1;
            if (block > BinAt(0)->size || block == 0)
            {
                break;
            }

            while ((block & BinAt(0)->size) == 0)
            {
                index += BinBlockWidth;
                block <<= 1;
            }
        }
    }

    // The top chunk, always left with a remainder
    s64 remainderSize = Difference(SizeOf(Top()), wanted);
    if (SizeOf(Top()) < wanted || remainderSize < MinimumSize)
    {
        ExtendTop(reent, wanted);
        remainderSize = Difference(SizeOf(Top()), wanted);
        if (SizeOf(Top()) < wanted || remainderSize < MinimumSize)
        {
            return nullptr;
        }
    }

    victim = Top();
    victim->size = wanted | PreviousInUse;
    BinAt(0)->next = At(victim, wanted);
    Top()->size = static_cast<u32>(remainderSize) | PreviousInUse;
    return MemoryOf(victim);
}

// _free_r: merged with the free chunks next to it, into the top chunk or a bin (or the last remainder's place)
void FreeReentrant(s32* reent, void* memory)
{
    if (memory == nullptr)
    {
        return;
    }

    Chunk* chunk = ChunkOf(memory);
    u32 head = chunk->size;
    u32 size = head & ~PreviousInUse;
    Chunk* next = At(chunk, size);
    u32 nextSize = SizeOf(next);
    if (next == Top())
    {
        size += nextSize;
        if ((head & PreviousInUse) == 0)
        {
            u32 previousSize = chunk->previousSize;
            chunk = At(chunk, -previousSize);
            size += previousSize;
            Unlink(chunk);
        }

        chunk->size = size | PreviousInUse;
        BinAt(0)->next = chunk;
        if (size >= g_MallocTrimThreshold)
        {
            Trim(reent, static_cast<u32>(g_MallocTopPad));
        }

        return;
    }

    next->size = nextSize;
    bool lastRemainder = false;
    if ((head & PreviousInUse) == 0)
    {
        u32 previousSize = chunk->previousSize;
        chunk = At(chunk, -previousSize);
        size += previousSize;
        if (chunk->next == LastRemainder())
        {
            lastRemainder = true;
        }
        else
        {
            Unlink(chunk);
        }
    }

    if ((At(next, nextSize)->size & PreviousInUse) == 0)
    {
        size += nextSize;
        if (!lastRemainder && next->next == LastRemainder())
        {
            lastRemainder = true;
            LinkLastRemainder(chunk);
        }
        else
        {
            Unlink(next);
        }
    }

    chunk->size = size | PreviousInUse;
    At(chunk, size)->previousSize = size;
    if (!lastRemainder)
    {
        FrontLink(chunk, size);
    }
}

// malloc_trim: whole pages of the top chunk past pad go back, when nothing else moved the break
s32 Trim(s32* reent, u32 pad)
{
    u64 topSize = SizeOf(Top());
    s64 extra = static_cast<s64>(((topSize - pad - MinimumSize + (PageSize - 1)) / PageSize - 1) * PageSize);
    if (extra < static_cast<s64>(PageSize))
    {
        return 0;
    }

    u8* end = static_cast<u8*>(SbrkReentrant(reent, 0));
    if (end != reinterpret_cast<u8*>(Top()) + static_cast<u32>(topSize))
    {
        return 0;
    }

    s32 released = static_cast<s32>(extra);
    if (SbrkReentrant(reent, -released) == reinterpret_cast<void*>(-1))
    {
        end = static_cast<u8*>(SbrkReentrant(reent, 0));
        s32 left = end - reinterpret_cast<u8*>(Top());
        if (left >= static_cast<s32>(MinimumSize))
        {
            g_MallocSbrked = end - g_MallocSbrkBase;
            Top()->size = static_cast<u32>(left) | PreviousInUse;
        }

        return 0;
    }

    Top()->size = static_cast<u32>(topSize - extra) | PreviousInUse;
    g_MallocSbrked -= released;
    return 1;
}

// _sbrk_r: errno set from the global errno (which sbrk leaves alone: it sets the block's own)
void* SbrkReentrant(s32* reent, s32 increment)
{
    g_Errno = 0;
    void* end = RetailLibc::Sbrk(increment);
    if (end == reinterpret_cast<void*>(-1) && g_Errno != 0)
    {
        *reent = g_Errno;
    }

    return end;
}

// The break moved past the kernel's end of the heap fails with ENOMEM
void* RetailLibc::Sbrk(s32 increment)
{
    s32 enabled = DIntr();
    u8* end = g_HeapEnd;
    u8* moved = end + increment;
    if (static_cast<u8*>(EndOfHeap()) < moved)
    {
        *ErrorNumber() = OutOfMemory;
        if (enabled != 0)
        {
            EIntr();
        }

        return reinterpret_cast<void*>(-1);
    }

    g_HeapEnd = moved;
    if (enabled != 0)
    {
        EIntr();
    }

    return end;
}

void* RetailLibc::Malloc(u32 size)
{
    return MallocReentrant(g_MallocReent, size);
}

void RetailLibc::Free(void* memory)
{
    FreeReentrant(g_MallocReent, memory);
}

// The C library's names for them: newlib's sprintf and PS2SDK's libraries use this heap (malloc and free took a semaphore,
// for threads nothing has)
extern "C"
{
    void* malloc(size_t size) __attribute__((alias("FUN_002c7a88")));
    void free(void* memory) __attribute__((alias("FUN_002c7ad8")));
    void* _malloc_r(void* reent, size_t size) __attribute__((alias("FUN_002cc308")));
    void _free_r(void* reent, void* memory) __attribute__((alias("FUN_002cca38")));

    // Sony's newlib had no realloc: newlib's string formatting only grows a buffer with it for asprintf, which nothing calls
    void* _realloc_r(void* reent, void* memory, size_t size)
    {
        if (memory == nullptr)
        {
            return MallocReentrant(static_cast<s32*>(reent), size);
        }

        u32 kept = SizeOf(ChunkOf(memory)) - SizeFieldSize;
        void* moved = MallocReentrant(static_cast<s32*>(reent), size);
        if (moved != nullptr)
        {
            RetailLibc::MemoryCopy(moved, memory, kept < size ? kept : size);
            FreeReentrant(static_cast<s32*>(reent), memory);
        }

        return moved;
    }
}
