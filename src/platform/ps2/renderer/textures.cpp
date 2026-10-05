#include "renderer.h"

#include "game/disk.h"
#include "game/memory.h"

#include <bit>
#include <libgs.h>

namespace
{
// 20 slots of 64 KB from the top of GS memory down (their addresses in BITBLTBUF's units of 64 words, 256 bytes), the large
// textures' one below them
constexpr u32 SlotCount = 0x14;
constexpr u32 FirstSlotAddress = 0x3F0000;
constexpr u32 SlotStep = 0x10000;
constexpr u32 LargeSlotAddress = 0x3C0000 - 0x140000;
constexpr u32 BlockShift = 8;
// Textures 3 times 64 pixels wide or more take the large textures' slot, the narrower ones the ring's
constexpr u32 LargeTextureWidth = 3;
// The ring's next slot taken is a free one or one last used more than the slot count less two uses ago, looked for two rounds
// at most
constexpr u32 UseMargin = 2;
constexpr u32 MaxRounds = 2;

// The upload: a CNT tag of VIF1's DIRECT of two quadwords, a GIF tag of one A+D write (BITBLTBUF: the slot's address, the
// buffer's width and the format) and a CALL of the texture's transfer chain
constexpr u32 UploadQuadwords = 2;
constexpr u32 BitBltAddressShift = 32;
constexpr u32 BitBltWidthShift = 48;
constexpr u32 BitBltFormatShift = 56;

// The texture's registers: TEXFLUSH, TEX0 (CLD 1: the palette's loaded), MIPTBP1 and MIPTBP2, each the first context's (the
// second's is one more). TEX0's and MIPTBPs' fields have the texture's values ORed in unmasked: one too wide runs into the next
constexpr u32 Tex0WidthShift = 14;
constexpr u32 Tex0FormatShift = 20;
constexpr u32 Tex0WidthPowerShift = 26;
constexpr u32 Tex0HeightPowerShift = 30;
constexpr u32 Tex0HasAlphaShift = 34;
constexpr u32 Tex0FunctionShift = 35;
constexpr u32 Tex0PaletteShift = 37;
// A MIPTBP has three levels' addresses and widths, 20 bits a level
constexpr u32 MipLevelsPerRegister = 3;
constexpr u32 MipLevelShift = 20;
constexpr u32 MipWidthShift = 14;
// Textures of 5 levels or more have mips past the third
constexpr u32 SecondMipLevels = 5;
constexpr u32 FirstMips = 1;
constexpr u32 SecondMips = 4;

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
    u64 value = 0;
    for (u32 level = 0; level < MipLevelsPerRegister; level++)
    {
        u32 shift = level * MipLevelShift;
        value |= static_cast<u64>(base + texture->levelOffsets[first + level]) << shift;
        value |= static_cast<u64>(texture->levelWidths[first + level]) << (shift + MipWidthShift);
    }

    return value;
}

// TEXFLUSH and the texture's TEX0 (MIPTBP1 and 2 for its mips) as A+D writes for its slot: returns how many
u32 WriteTexturePairs(const Texture* texture, GsWrite* writes)
{
    const u64 LoadPalette = std::bit_cast<u64>(GS_TEX0{.clut_loadmode = 1});
    u32 base = texture->slot->gsAddress;
    u64 context = texture->secondContext != 0 ? 1 : 0;
    writes[0] = {1, GsTexFlush};
    u64 tex0 = static_cast<u64>(base + texture->levelOffsets[0]);
    tex0 |= static_cast<u64>(texture->levelWidths[0]) << Tex0WidthShift;
    tex0 |= static_cast<u64>(texture->pixelFormat) << Tex0FormatShift;
    tex0 |= static_cast<u64>(texture->widthPower) << Tex0WidthPowerShift;
    tex0 |= static_cast<u64>(texture->heightPower) << Tex0HeightPowerShift;
    tex0 |= static_cast<u64>(texture->hasAlpha) << Tex0HasAlphaShift;
    tex0 |= static_cast<u64>(texture->function) << Tex0FunctionShift;
    tex0 |= static_cast<u64>(base + texture->paletteOffset) << Tex0PaletteShift;
    writes[1] = {tex0 | LoadPalette, GsTex0 + context};
    if (texture->levels != 1)
    {
        writes[2] = {MipRegister(texture, base, FirstMips), GsMipTbp1 + context};
        if (texture->levels >= SecondMipLevels)
        {
            writes[3] = {MipRegister(texture, base, SecondMips), GsMipTbp2 + context};
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
    void MakeTextureSlots(void*)
    {
        g_TextureSlotNext = 0;
        g_TextureSlotCount = SlotCount;
        g_TextureTime = 0;
        g_TextureSlots = NewArray<TextureSlot>(SlotCount);
        u32 address = FirstSlotAddress;
        for (u32 i = 0; i < g_TextureSlotCount; i++)
        {
            TextureSlot& slot = g_TextureSlots[i];
            slot.buckets = 0;
            slot.gsAddress = address >> BlockShift;
            slot.used = 0;
            slot.owner = nullptr;
            address -= SlotStep;
        }

        g_LargeTextureSlot.owner = nullptr;
        g_LargeTextureSlot.buckets = 0;
        g_LargeTextureSlot.gsAddress = LargeSlotAddress >> BlockShift;
        g_LargeTextureSlot.used = 0;
    }

    u8* UploadTexture(void* context, u8* packet, Texture* texture, u32 bucket)
    {
        // The bucket's bit in the slot's word of buckets sent to (the shift's count masked like the hardware's)
        u32 bit = 1u << (bucket & ShiftMask);
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
            return WriteTextureUpload(context, packet, texture, slot);
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
            return WriteTextureUpload(context, packet, texture, slot);
        }

        // The ring's next slot free enough, but the one the bucket's last texture took
        RenderBucket& frameBucket = g_FrameBuckets.buckets[bucket];
        u32 count = g_TextureSlotCount;
        u32 index = g_TextureSlotNext;
        u32 time = g_TextureTime;
        u32 rounds = 0;
        bool found = false;
        do
        {
            TextureSlot& candidate = g_TextureSlots[index];
            if ((candidate.used < time - count + UseMargin || candidate.owner == nullptr) &&
                frameBucket.lastSlotAddress != candidate.gsAddress)
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
                frameBucket.lastSlotAddress = candidate.gsAddress;
                slot = &candidate;
            }

            if (++index == count)
            {
                index = 0;
                rounds++;
            }
        } while (!found && rounds != MaxRounds);

        g_TextureTime = time;
        g_TextureSlotNext = index;
        if (slot == nullptr)
        {
            return packet;
        }

        return WriteTextureUpload(context, packet, texture, slot);
    }

    u8* WriteTextureUpload(void*, u8* packet, Texture* texture, TextureSlot* slot)
    {
        u8* transfer = DiskLoadedMemory(GetDiskManager(), &texture->transfer);
        auto* tag = reinterpret_cast<u32*>(packet);
        tag[0] = CountTag | UploadQuadwords;
        tag[1] = 0;
        tag[2] = 0;
        tag[3] = VifDirect | UploadQuadwords;
        // The buffer's width is the texture's for 32 bit pixels, 64 pixels for the others
        u64 width = texture->pixelFormat != GS_TEX_32 ? 1 : texture->levelWidths[0];
        auto* gif = reinterpret_cast<GsWrite*>(packet + 0x10);
        gif[0] = {AddressDataTag(1), GifAddressData};
        u64 destination = static_cast<u64>(slot->gsAddress) << BitBltAddressShift | width << BitBltWidthShift |
                          static_cast<u64>(texture->uploadFormat) << BitBltFormatShift;
        gif[1] = {destination, GsBitBltBuf};
        auto* call = reinterpret_cast<u32*>(packet + 0x30);
        call[0] = CallTag;
        call[1] = Address(transfer);
        call[2] = 0;
        call[3] = 0;
        return packet + 0x40;
    }

    void FreeTextureSlots(void* context, u32 keepTime)
    {
        for (u32 i = 0; i < g_TextureSlotCount; i++)
        {
            FreeTextureSlot(i);
        }

        FreeLargeTextureSlot(context);
        g_TextureSlotNext = 0;
        if (keepTime == 0)
        {
            g_TextureTime = 0;
        }
    }

    void FreeTextureSlot(u32 index)
    {
        Free(g_TextureSlots[index]);
    }

    void FreeLargeTextureSlot(void*)
    {
        Free(g_LargeTextureSlot);
    }

    u32 ClaimFirstTextureSlot()
    {
        Free(g_TextureSlots[0]);
        return g_TextureSlots[0].gsAddress;
    }

    u8* WriteTextureRegisters(void*, u8* packet, const Texture* texture)
    {
        u32 quadwords = WriteTexturePairs(texture, reinterpret_cast<GsWrite*>(packet + 0x20)) + 1;
        // A CNT tag of VIF1's DIRECT of them, after a FLUSHA
        auto* tag = reinterpret_cast<u32*>(packet);
        tag[0] = quadwords | CountTag;
        tag[1] = 0;
        tag[2] = VifFlushA;
        tag[3] = quadwords | VifDirect;
        auto* gif = reinterpret_cast<u64*>(packet + 0x10);
        gif[1] = GifAddressData;
        gif[0] = AddressDataTag(quadwords - 1);
        return packet + quadwords * 0x10 + 0x10;
    }

    u8* UploadShaderTexture(const Material* material, u8* packet, u32 shader, u32 bucket)
    {
        if (material->shaders[shader]->texture == nullptr)
        {
            return packet;
        }

        return UploadTexture(&g_TextureUploadContext, packet, TextureOf(material->shaders[shader]), bucket);
    }

    u8* WriteShaderTextureRegisters(const Material* material, u8* packet, u32 shader)
    {
        return WriteTextureRegisters(&g_TextureUploadContext, packet, TextureOf(material->shaders[shader]));
    }

    u8* WriteTextureRegistersToVu(void*, u8* packet, const Texture* texture, u32* counter, u32 tagPlace)
    {
        u32 pairs = WriteTexturePairs(texture, reinterpret_cast<GsWrite*>(packet + 0x10));
        // A CNT tag of an UNPACK of them, after a FLUSHA
        auto* tag = reinterpret_cast<u32*>(packet);
        tag[0] = pairs | CountTag;
        tag[1] = 0;
        tag[2] = VifFlushA;
        tag[3] = VifUnpackTo(VifUnpackV4Count, *counter, pairs);
        *counter += pairs;
        auto* at = reinterpret_cast<u32*>(packet + 0x10 + pairs * 0x10);
        at[0] = CountTag | 1;
        at[1] = 0;
        at[2] = 0;
        at[3] = tagPlace | VifUnpackV4;
        auto* gif = reinterpret_cast<u64*>(at + 4);
        gif[1] = GifAddressData;
        gif[0] = AddressDataTag(*counter - tagPlace - 1);
        return reinterpret_cast<u8*>(at + 8);
    }

    u32 TextureRegisterCount(void*, const Texture* texture)
    {
        return PairCount(texture);
    }
}
