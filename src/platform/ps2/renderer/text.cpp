#include "renderer.h"

#include "game/font.h"

namespace
{
// A packet's lines' glyphs fit 0x260 bytes with 4 bytes of each line
constexpr s32 PacketGlyphBytes = 0x260;
// Where the font's glyphs go in VU1's memory; a text's header and lines go to its double buffer's place (FLG), and its glyphs
// after them as V4-8 elements
constexpr u32 VuGlyphsAddress = 0x36;
constexpr u32 HeaderUnpack = VifUnpackV4Count | VifTops;
constexpr u32 GlyphUnpack = VifUnpackV4Bytes | VifTops;
// A glyph's page is its font page's index plus this
constexpr s32 GlyphPageBase = 1;

Material* PageMaterial(const Font* font, s32 page)
{
    return font->materials[page]->material;
}

s32 RoundedUp4(s32 count)
{
    return count + (-count & 3);
}

// How many lines from the first one packet takes, and their glyphs' bytes (rounded up to quadwords)
s32 TakeLines(const TextLayout& layout, s32 first, s32* bytes)
{
    s32 count = 0;
    *bytes = 0;
    for (s32 line = first; line < layout.lineCount; line++)
    {
        s32 size = *bytes + RoundedUp4(layout.counts[line]);
        if (size + count * 4 > PacketGlyphBytes)
        {
            break;
        }

        *bytes = size;
        count++;
    }

    if ((*bytes & 0xF) != 0)
    {
        s32 value = *bytes >= 0 ? *bytes : *bytes + 0xF;
        *bytes = (value >> 4 << 4) + 0x10;
    }

    return count;
}
}

extern "C"
{
    // Where the text packets in progress go on
    extern u8* g_TextNext RETAIL(D_0030AB50);

    // The material's writer takes text: when it starts, the font's glyphs are sent first (to VU1's 0x36, a FLUSHA after them)
    void BeginTextPacket(const Font* font, Material* material) RETAIL(FUN_001aafb0);
    // The text item's packets at the position (in the GS's coordinates), recorded in packets
    void WriteTextPackets(const Font* font, const TextItem* item, const Vector2* position, TextPackets* packets)
        RETAIL(FUN_001aa400);

    void BeginTextPacket(const Font* font, Material* material)
    {
        RenderBucket& writer = material->writer;
        if (writer.first == nullptr)
        {
            StartMaterialWriter(material);
            s32 count = font->glyphCount;
            u32* at = BeginPacket(writer);
            g_TextNext = reinterpret_cast<u8*>(at);
            at[0] = (count + 1) | ReferenceTag;
            at[1] = Address(font->glyphs);
            at[2] = 0;
            at[3] = VifUnpackTo(VifUnpackV4Count, VuGlyphsAddress, count);
            g_TextNext = reinterpret_cast<u8*>(at + 4);
            return;
        }

        g_TextNext = reinterpret_cast<u8*>(BeginPacket(writer));
    }

    void WriteTextPackets(const Font* font, const TextItem* item, const Vector2* position, TextPackets* packets)
    {
        const char* text = item->text;
        Vector2 scale;
        FontScale(font, &scale);
        Vector2 size;
        CopyVector2(&size, &item->scale);
        size.x = size.x * scale.x;
        size.y = size.y * scale.y;
        TextLayout layout;
        layout.pages[0] = 0;
        layout.pages[1] = 0;
        layout.pages[2] = 0;
        layout.pages[3] = 0;
        RenderText(font, text, &size, &layout);
        Vector2 cursor;
        CopyVector2(&cursor, position);
        TextAlignment alignment = item->alignment;
        if (alignment.top == 0)
        {
            if (alignment.bottom != 0)
            {
                cursor.y = cursor.y - layout.height;
            }
            else
            {
                cursor.y = cursor.y - layout.height * 0.5f;
            }
        }

        s32 first = 0;
        s32 taken = 0;
        s32 bytes;
        s32 count = TakeLines(layout, first, &bytes);
        while (count != 0)
        {
            // The header: its tag (its size written last), the colour (a byte a word), the size, the count of lines (the size's
            // fourth word left as the buffer had it)
            auto* tag = reinterpret_cast<u32*>(g_TextNext);
            tag[0] = 0;
            tag[1] = 0;
            tag[2] = 0;
            tag[3] = VifUnpackTo(HeaderUnpack, 0, count + 2);
            u32* at = tag + 4;
            u32 quadwords = 2;
            u32 colour = item->colour;
            at[0] = colour & 0xFF;
            at[1] = colour >> 8 & 0xFF;
            at[2] = colour >> 16 & 0xFF;
            at[3] = colour >> 24;
            reinterpret_cast<f32*>(at)[4] = size.x;
            reinterpret_cast<f32*>(at)[5] = size.y;
            at[6] = count;
            at += 8;

            // Each line: where it starts (by its alignment) and its glyphs' words (the fourth word left)
            for (s32 line = 0; line < count; line++)
            {
                Vector2 extent;
                Vector2 start;
                CopyVector2(&extent, &layout.lines[first + line]);
                CopyVector2(&start, &cursor);
                if (alignment.left == 0)
                {
                    if (alignment.right != 0)
                    {
                        start.x = start.x - extent.x;
                    }
                    else
                    {
                        start.x = start.x - extent.x * 0.5f;
                    }
                }

                reinterpret_cast<f32*>(at)[0] = start.x;
                reinterpret_cast<f32*>(at)[1] = start.y;
                at[2] = static_cast<u32>(RoundedUp4(layout.counts[first + line]) >> 2);
                at += 4;
                quadwords++;
                cursor.y = cursor.y + extent.y;
            }

            // The glyphs as bytes after an UNPACK of them, each line's padded to a word
            at[0] = VifUnpackTo(GlyphUnpack, count + 2, bytes >> 2);
            auto* glyphs = reinterpret_cast<u8*>(at);
            s32 offset = 4;
            for (s32 line = 0; line < count; line++)
            {
                const u8* from = layout.glyphs + taken;
                s32 glyphCount = layout.counts[first + line];
                taken += glyphCount;
                while (glyphCount-- != 0)
                {
                    glyphs[offset++] = *from++;
                    if (offset >= 0x10)
                    {
                        offset = 0;
                        quadwords++;
                        glyphs += 0x10;
                    }
                }

                for (s32 pad = -offset & 3; pad != 0; pad--)
                {
                    glyphs[offset++] = 0;
                }

                if (offset >= 0x10)
                {
                    offset = 0;
                    quadwords++;
                    glyphs += 0x10;
                }
            }

            if (offset > 0)
            {
                for (; offset < 0x10; offset++)
                {
                    glyphs[offset] = 0;
                }

                quadwords++;
                glyphs += 0x10;
            }

            at = reinterpret_cast<u32*>(glyphs);
            at[0] = 0;
            at[1] = 0;
            at[2] = 0;
            at[3] = VifMscnt;
            g_TextNext = reinterpret_cast<u8*>(at + 4);
            quadwords++;
            *reinterpret_cast<u64*>(tag) = quadwords | CountTag;
            packets->sizes[packets->count] = quadwords;
            first += count;
            packets->tags[packets->count] = tag;
            packets->count++;
            count = TakeLines(layout, first, &bytes);
        }

        for (u32 page = 0; page < 4; page++)
        {
            if (layout.pages[page] == 1)
            {
                packets->pages[page] = 1;
            }
        }
    }
}

void Font::Begin()
{
    BeginTextPacket(this, PageMaterial(this, 0));
}

void Font::Draw(const TextItem* item, TextPackets* packets)
{
    Vector2 position;
    CopyVector2(&position, &item->position);
    position.x = position.x * static_cast<f32>(g_RendererWidth) * 16.0f;
    position.y = position.y * static_cast<f32>(g_RendererHeight) * 16.0f;
    WriteTextPackets(this, item, &position, packets);
}

// The first page's packet finished; each other page with glyphs of its page (a glyph's page is the font page's index + 1) sends
// the same packets with its material, by REF
void Font::End(TextPackets* packets)
{
    Material* material = PageMaterial(this, 0);
    EndPacket(material->writer, g_TextNext);
    g_TextNext = nullptr;
    for (s32 page = 1; page < pageCount; page++)
    {
        if (packets->pages[page + GlyphPageBase] == 0)
        {
            continue;
        }

        material = PageMaterial(this, page);
        BeginTextPacket(this, material);
        auto* at = reinterpret_cast<u32*>(g_TextNext);
        for (s32 i = 0; i < packets->count; i++)
        {
            const u32* tag = packets->tags[i];
            at[0] = packets->sizes[i] | ReferenceTag;
            at[1] = Address(tag + 4);
            at[2] = 0;
            at[3] = tag[3];
            at += 4;
        }

        g_TextNext = reinterpret_cast<u8*>(at);
        EndPacket(material->writer, g_TextNext);
        g_TextNext = nullptr;
    }
}

namespace Platform::Graphics
{
void SetFontPage(Material* material, u32 page)
{
    material->shaders[0]->fontPage = static_cast<u8>(page);
}
}
