#include "game/font.h"

#include "game/memory.h"
#include "game/renderer.h"
#include "game/stream.h"
#include "game/string.h"
#include "platform/graphics.h"

namespace
{
// The character's glyph: the first when the font has none
u32 GlyphIndex(const Font* font, u32 character)
{
    s32 index = static_cast<s32>(character & 0xFF) - font->firstCharacter;
    if (index < 0 || index >= font->glyphCount)
    {
        return 0;
    }

    return static_cast<u32>(index) & 0xFF;
}

// A glyph's page is in the low bits of its first byte (x's lowest bits)
constexpr u8 GlyphPageMask = 3;

u8 GlyphPage(const Vector4& glyph)
{
    return *reinterpret_cast<const u8*>(&glyph.x) & GlyphPageMask;
}

// A glyph's width and height are the GS's sixteenths of a pixel
constexpr f32 PixelsPerGlyphUnit = 0.0625f;
// Where a text starts a new line
constexpr char NewLine = '~';

// The glyphs as new[] makes an array of a class: their count in the 16 bytes before them
constexpr u32 ArrayCookie = 0x10;
// The VIF code after the glyphs: MSCAL 0
constexpr u32 GlyphsEnd = 0x14000000;

Vector4* NewGlyphs(u32 count)
{
    auto* block = static_cast<u8*>(MemoryAllocate2(count * sizeof(Vector4) + ArrayCookie));
    *reinterpret_cast<u32*>(block) = count;
    return reinterpret_cast<Vector4*>(block + ArrayCookie);
}
}

Font* Font::ConstructBase(Font* font)
{
    font->glyphCount = 0;
    font->height = 1.0f;
    font->firstCharacter = 0;
    font->glyphs = nullptr;
    font->width = 1.0f;
    return font;
}

void Font::DestroyBase(u32 flags)
{
    if (glyphs != nullptr)
    {
        MemoryDeallocate_(reinterpret_cast<u8*>(glyphs) - ArrayCookie);
    }

    if ((flags & FreeAfterDestroy) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

void Font::BaseDestroy(u32 flags)
{
    vtable = g_FontBaseVTable;
    DestroyBase(DestroyOnly);
    if ((flags & FreeAfterDestroy) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

Font* Font::Construct(Font* font)
{
    font->vtable = g_FontBaseVTable;
    ConstructBase(font);
    font->pageCount = 0;
    font->vtable = g_FontVTable;
    for (u32 page = 0; page < MaxPages; page++)
    {
        font->materials[page] = nullptr;
        font->textures[page] = nullptr;
    }

    return font;
}

void Font::Destroy(u32 flags)
{
    vtable = g_FontVTable;
    ReleasePages();
    vtable = g_FontBaseVTable;
    DestroyBase(DestroyOnly);
    if ((flags & FreeAfterDestroy) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

void Font::ReleasePages()
{
    for (s32 page = 0; page < pageCount; page++)
    {
        ReleaseMaterial(materials[page]);
        materials[page] = nullptr;
        ReleaseTexture(textures[page]);
        textures[page] = nullptr;
    }
}

void Font::Read(const char* name)
{
    ReleasePages();
    String path;
    StringConstruct(&path, name);
    String withExtension;
    StringConstructCopy(&withExtension, &path);
    StringAppend(&withExtension, g_FontExtension);
    String file;
    StringConstructCopy(&file, &withExtension);
    StringDelete(&withExtension, DestroyOnly);
    MemoryStream reader;
    MemoryStream::ConstructFromFile(&reader, file.string, false);
    StringDestroy(&file);
    Stream* stream = &reader;
    stream->ReadU32(reinterpret_cast<u32*>(&pageCount));
    for (s32 page = 0; page < pageCount; page++)
    {
        u32 textureId;
        u32 materialId;
        stream->ReadS32(reinterpret_cast<s32*>(&textureId));
        stream->ReadS32(reinterpret_cast<s32*>(&materialId));
        textures[page] = TextureFromStream(textureId, stream);
        MaterialResource* material = MaterialFromStream(materialId, stream);
        materials[page] = material;
        Platform::Graphics::SetFontPage(material->material, static_cast<u32>(page));
    }

    stream->ReadU32(reinterpret_cast<u32*>(&glyphCount));
    stream->ReadU32(reinterpret_cast<u32*>(&firstCharacter));
    glyphs = NewGlyphs(static_cast<u32>(glyphCount + 1));
    stream->Read(glyphs, static_cast<u32>(glyphCount) * sizeof(Vector4), 1);
    auto* end = reinterpret_cast<u32*>(&glyphs[glyphCount]);
    end[0] = 0;
    end[1] = 0;
    end[2] = 0;
    end[3] = GlyphsEnd;
    reader.Destroy(DestroyOnly);
    StringDestroy(&path);
}

extern "C"
{
    void FontScale(const Font* font, Vector2* scale)
    {
        const Vector4& letterA = font->glyphs[GlyphIndex(font, 'a')];
        f32 width = letterA.z * PixelsPerGlyphUnit;
        f32 height = letterA.w * PixelsPerGlyphUnit;
        width = width / static_cast<f32>(g_RendererWidth);
        height = height / static_cast<f32>(g_RendererHeight);
        scale->x = font->width / width;
        scale->y = font->height / height;
    }

    void RenderText(const Font* font, const char* text, const Vector2* scale, TextLayout* layout)
    {
        const Vector4& space = font->glyphs[GlyphIndex(font, ' ')];
        layout->lineCount = 0;
        layout->height = 0.0f;
        u32 glyphs = 0;
        while (*text != '\0')
        {
            s32 count = 0;
            f32 width = 0.0f;
            f32 height = space.w;
            if (*text == NewLine)
            {
                // An empty line is the space's height more for the line before it (and the text)
                text++;
                s32 last = layout->lineCount != 0 ? layout->lineCount - 1 : 0;
                layout->lines[last].y = layout->lines[last].y + height * scale->y;
            }
            else
            {
                do
                {
                    u32 index = GlyphIndex(font, static_cast<u8>(*text));
                    const Vector4& glyph = font->glyphs[index];
                    if (height < glyph.w)
                    {
                        height = glyph.w;
                    }

                    width = width + glyph.z;
                    layout->glyphs[glyphs++] = static_cast<u8>(index);
                    text++;
                    layout->pages[GlyphPage(glyph)] = 1;
                    count++;
                } while (*text != '\0' && *text != NewLine);

                if (*text == NewLine)
                {
                    text++;
                }
            }

            height = height * scale->y;
            layout->height = layout->height + height;
            if (count > 0)
            {
                layout->lines[layout->lineCount].x = width * scale->x;
                layout->lines[layout->lineCount].y = height;
                layout->counts[layout->lineCount] = count;
                layout->lineCount++;
            }
        }
    }
}
