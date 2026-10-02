#include "renderer.h"

#include "game/disk.h"
#include "game/memory.h"

namespace
{
// 20 slots of 64 KB from the top of GS memory down (their addresses in BITBLTBUF's units of 64 words), the large textures' one
// below them
constexpr u32 SlotCount = 0x14;
constexpr u32 FirstSlotAddress = 0x3F0000;
constexpr u32 SlotStep = 0x10000;
constexpr u32 LargeSlotAddress = 0x3C0000 - 0x140000;
// Textures 3 times 64 pixels wide or more take the large textures' slot, the narrower ones the ring's
constexpr u32 LargeTextureWidth = 3;

// The upload: a CNT tag of VIF1's DIRECT of two quadwords, a GIF tag of one A+D pair (BITBLTBUF), and a CALL of the texture's
// transfer chain
constexpr u64 GifOneRegister = 0x8000ull << 45 | 0x8001;
constexpr u64 AddressAndData = 0xE;
constexpr u64 BitBltBuf = 0x50;

// The texture's registers: a GIF tag of one A+D register (its loops added) that ends the packet, TEXFLUSH, TEX0 (CLD 1: the
// palette's loaded), MIPTBP1 and MIPTBP2, each the first context's (the second's is one more)
constexpr u64 GifRegisters = 0x8000ull << 45 | 0x8000;
constexpr u64 TexFlush = 0x3F;
constexpr u64 Tex0 = 0x06;
constexpr u64 MipTbp1 = 0x34;
constexpr u64 MipTbp2 = 0x36;
constexpr u64 LoadPalette = 1ull << 61;
// Textures of 5 levels or more have mips past the third
constexpr u32 SecondMipLevels = 5;

// The number of the texture's registers: TEXFLUSH, TEX0 and MIPTBP1 and 2 for its mips
u32 PairCount(const Texture* texture)
{
    if (texture->levels == 1)
    {
        return 2;
    }

    return texture->levels < SecondMipLevels ? 3 : 4;
}

// MIPTBP1 or 2: three levels' addresses and widths
u64 MipRegister(const Texture* texture, u32 base, u32 first)
{
    u64 value = static_cast<u64>(base + texture->levelOffsets[first]);
    value |= static_cast<u64>(texture->levelWidths[first]) << 14;
    value |= static_cast<u64>(base + texture->levelOffsets[first + 1]) << 20;
    value |= static_cast<u64>(texture->levelWidths[first + 1]) << 34;
    value |= static_cast<u64>(base + texture->levelOffsets[first + 2]) << 40;
    value |= static_cast<u64>(texture->levelWidths[first + 2]) << 54;
    return value;
}

// TEXFLUSH and the texture's TEX0 (MIPTBP1 and 2 for its mips) as A+D pairs for its slot: returns how many
u32 WriteTexturePairs(const Texture* texture, u64* pairs)
{
    u32 base = texture->slot->gsAddress;
    u64 context = texture->secondContext != 0 ? 1 : 0;
    pairs[0] = 1;
    pairs[1] = TexFlush;
    u64 tex0 = static_cast<u64>(base + texture->levelOffsets[0]);
    tex0 |= static_cast<u64>(texture->levelWidths[0]) << 14;
    tex0 |= static_cast<u64>(texture->pixelFormat) << 20;
    tex0 |= static_cast<u64>(texture->widthPower) << 26;
    tex0 |= static_cast<u64>(texture->heightPower) << 30;
    tex0 |= static_cast<u64>(texture->hasAlpha) << 34;
    tex0 |= static_cast<u64>(texture->function) << 35;
    tex0 |= static_cast<u64>(base + texture->paletteOffset) << 37;
    pairs[2] = tex0 | LoadPalette;
    pairs[3] = Tex0 + context;
    if (texture->levels != 1)
    {
        pairs[4] = MipRegister(texture, base, 1);
        pairs[5] = MipTbp1 + context;
        if (texture->levels >= SecondMipLevels)
        {
            pairs[6] = MipRegister(texture, base, 4);
            pairs[7] = MipTbp2 + context;
        }
    }

    return PairCount(texture);
}

// The slot's texture is forgotten, and the slot's uses
void Free(TextureSlot& slot)
{
    if (slot.owner != nullptr)
    {
        slot.owner->slot = nullptr;
    }

    slot.buckets = 0;
    slot.owner = nullptr;
    slot.used = 0;
}
}

extern "C"
{
    void FUN_001bc8f0(void*)
    {
        g_TextureSlotNext = 0;
        g_TextureSlotCount = SlotCount;
        g_TextureTime = 0;
        // GCC 2.9x's array new: the count ahead of the slots
        auto* block = static_cast<u32*>(MemoryAllocate2(0x10 + SlotCount * sizeof(TextureSlot)));
        block[0] = SlotCount;
        g_TextureSlots = reinterpret_cast<TextureSlot*>(block + 4);
        u32 address = FirstSlotAddress;
        for (u32 i = 0; i < g_TextureSlotCount; i++)
        {
            TextureSlot& slot = g_TextureSlots[i];
            slot.buckets = 0;
            slot.gsAddress = address >> 8;
            slot.used = 0;
            slot.owner = nullptr;
            address -= SlotStep;
        }

        g_LargeTextureSlot.owner = nullptr;
        g_LargeTextureSlot.buckets = 0;
        g_LargeTextureSlot.gsAddress = LargeSlotAddress >> 8;
        g_LargeTextureSlot.used = 0;
    }

    u8* FUN_001bc9e0(void* context, u8* packet, Texture* texture, u32 bucket)
    {
        u32 bit = 1u << (bucket & 0x1F);
        TextureSlot* slot = texture->slot;
        if (slot != nullptr)
        {
            if ((slot->buckets & bit) != 0)
            {
                slot->used = g_TextureTime++;
                return packet;
            }

            slot->used = g_TextureTime++;
            slot->buckets |= bit;
            return SetTextureDma_(context, packet, texture, slot);
        }

        if (texture->levelWidths[0] >= LargeTextureWidth)
        {
            slot = &g_LargeTextureSlot;
            if (slot->owner != nullptr)
            {
                slot->owner->slot = nullptr;
            }

            slot->owner = texture;
            slot->used = g_TextureTime++;
            slot->buckets = bit;
            texture->slot = slot;
            return SetTextureDma_(context, packet, texture, slot);
        }

        // The ring's next slot not used for the last count - 2 uses (or free), but the one the bucket's last texture took, two
        // rounds at most
        RenderBucket& owner = g_FrameBuckets.buckets[bucket];
        u32 count = g_TextureSlotCount;
        u32 index = g_TextureSlotNext;
        u32 time = g_TextureTime;
        u32 rounds = 0;
        bool found = false;
        do
        {
            TextureSlot& candidate = g_TextureSlots[index];
            if ((candidate.used < time - count + 2 || candidate.owner == nullptr) && owner.unknown34 != candidate.gsAddress)
            {
                if (candidate.owner != nullptr)
                {
                    candidate.owner->slot = nullptr;
                }

                candidate.used = time++;
                candidate.owner = texture;
                candidate.buckets = bit;
                found = true;
                texture->slot = &candidate;
                owner.unknown34 = candidate.gsAddress;
                slot = &candidate;
            }

            if (++index == count)
            {
                index = 0;
                rounds++;
            }
        } while (!found && rounds != 2);

        g_TextureTime = time;
        g_TextureSlotNext = index;
        if (slot == nullptr)
        {
            return packet;
        }

        return SetTextureDma_(context, packet, texture, slot);
    }

    u8* SetTextureDma_(void*, u8* packet, Texture* texture, TextureSlot* slot)
    {
        u8* transfer = DiskLoadedMemory(GetDiskManager(), &texture->transfer);
        auto* tag = reinterpret_cast<u32*>(packet);
        tag[0] = CountTag | 2;
        tag[1] = 0;
        tag[2] = 0;
        tag[3] = VifDirect | 2;
        u64 width = texture->pixelFormat != 0 ? 1 : texture->levelWidths[0];
        auto* gif = reinterpret_cast<u64*>(packet + 0x10);
        gif[0] = GifOneRegister;
        gif[1] = AddressAndData;
        gif[2] = static_cast<u64>(slot->gsAddress) << 32 | width << 48 | static_cast<u64>(texture->uploadFormat) << 56;
        gif[3] = BitBltBuf;
        auto* call = reinterpret_cast<u32*>(packet + 0x30);
        call[0] = CallTag;
        call[1] = Address(transfer);
        call[2] = 0;
        call[3] = 0;
        return packet + 0x40;
    }

    void FUN_001c0f08(void* context, u32 keepTime)
    {
        for (u32 i = 0; i < g_TextureSlotCount; i++)
        {
            FUN_001c0f88(i);
        }

        FUN_001c0fb8(context);
        g_TextureSlotNext = 0;
        if (keepTime == 0)
        {
            g_TextureTime = 0;
        }
    }

    void FUN_001c0f88(u32 index)
    {
        Free(g_TextureSlots[index]);
    }

    void FUN_001c0fb8(void*)
    {
        Free(g_LargeTextureSlot);
    }

    u32 FUN_001c0fe0()
    {
        Free(g_TextureSlots[0]);
        return g_TextureSlots[0].gsAddress;
    }

    u8* WriteTextureRegisters(void*, u8* packet, const Texture* texture)
    {
        u32 quadwords = WriteTexturePairs(texture, reinterpret_cast<u64*>(packet + 0x20)) + 1;
        // A CNT tag of VIF1's DIRECT of them, after a FLUSHA
        auto* tag = reinterpret_cast<u32*>(packet);
        tag[0] = quadwords | CountTag;
        tag[1] = 0;
        tag[2] = VifFlushA;
        tag[3] = quadwords | VifDirect;
        auto* gif = reinterpret_cast<u64*>(packet + 0x10);
        gif[1] = AddressAndData;
        gif[0] = (quadwords - 1) | GifRegisters;
        return packet + quadwords * 0x10 + 0x10;
    }

    u8* FUN_001c0ab8(const Material* material, u8* packet, u32 shader, u32 bucket)
    {
        u8* texture = material->shaders[shader]->texture;
        if (texture == nullptr)
        {
            return packet;
        }

        return FUN_001bc9e0(&D_0030A820, packet, reinterpret_cast<Texture*>(texture + 0xC), bucket);
    }

    u8* FUN_001c0b00(const Material* material, u8* packet, u32 shader)
    {
        return WriteTextureRegisters(&D_0030A820, packet, reinterpret_cast<const Texture*>(material->shaders[shader]->texture + 0xC));
    }

    u8* WriteTextureRegistersToVu(void*, u8* packet, const Texture* texture, u32* counter, u32 tagPlace)
    {
        u32 pairs = WriteTexturePairs(texture, reinterpret_cast<u64*>(packet + 0x10));
        // A CNT tag of an UNPACK of them, after a FLUSHA
        auto* tag = reinterpret_cast<u32*>(packet);
        tag[0] = pairs | CountTag;
        tag[1] = 0;
        tag[2] = VifFlushA;
        tag[3] = *counter | pairs << 16 | VifUnpackV4Count;
        *counter += pairs;
        auto* at = reinterpret_cast<u32*>(packet + 0x10 + pairs * 0x10);
        at[0] = CountTag | 1;
        at[1] = 0;
        at[2] = 0;
        at[3] = tagPlace | VifUnpackV4;
        auto* gif = reinterpret_cast<u64*>(at + 4);
        gif[1] = AddressAndData;
        gif[0] = (*counter - tagPlace - 1) | GifRegisters;
        return reinterpret_cast<u8*>(at + 8);
    }

    u32 TextureRegisterCount(void*, const Texture* texture)
    {
        return PairCount(texture);
    }
}
