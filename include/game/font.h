#pragma once

#include "common.h"
#include "game/math.h"
#include "gcc2.h"
#include "game/graphicstables.h"

// A text a font draws: where (fractions of the screen), its size (glyphs scaled so the font's "a" has the font's size), its
// colour (RGBA bytes), its alignment (Align*) and the text ("~" starts a line)
struct TextItem
{
    Vector2 position;
    Vector2 scale;
    u32 colour;
    u32 flags;
    const char* text;
};

// The text laid out: each line's width and height, its count of glyphs, the glyphs (their indexes in the font) of every line one
// after another, the text's height, how many lines, and the glyphs' pages that are used (a glyph's page is 1 for the first page,
// 2 the second...)
struct TextLayout
{
    Vector2 lines[64];
    s32 counts[64];
    u8 glyphs[0x800];
    f32 height;
    s32 lineCount;
    u8 unknownB08[0xC];
    u8 pages[4];
};
CHECK_OFFSET(TextLayout, pages, 0xB14);

// What a frame's texts drawn with a font sent: each packet's size (quadwords) and where it is (its tag), and the pages used, which
// the other pages' materials send the same packets for
struct TextPackets
{
    s32 count;
    s32 sizes[0x80];
    u32* tags[0x80];
    u8 pages[4];
};
CHECK_OFFSET(TextPackets, pages, 0x404);

struct GameTexture;

// A font (a PSF file): its glyphs (a quadword each: the page in the first byte's low bits, the width and height in z and w, then a
// quadword of VIF codes the glyphs' upload runs: an MSCAL of VU1's program 0), the character of the first, the size its "a" is drawn at, its vtable,
// and its pages (a material and a texture each). The vtable's functions: the destructor, then the frame's texts begun, a text
// drawn and the texts finished
class Font
{
public:
    enum Align : u32
    {
        AlignTop = 1,
        AlignBottom = 4,
        AlignLeft = 0x10,
        AlignRight = 0x40,
    };

    s32 glyphCount;
    s32 firstCharacter;
    Vector4* glyphs;
    f32 width;
    f32 height;
    const GccVTableEntry* vtable;
    s32 pageCount;
    MaterialResource* materials[3];
    GameTexture* textures[3];
    u8 unknown34[0xC];

    // The base class's (D_002F6F80, its vtable at 0x14 too: 1 the destructor): no glyphs, the size 1; the glyphs freed (its vtable
    // left alone); the destructor
    static Font* ConstructBase(Font* font) RETAIL(FUN_001ac8d0);
    void DestroyBase(u32 flags) RETAIL(FUN_001ac850);
    void BaseDestroy(u32 flags) RETAIL(FUN_001ad200);
    // Without pages
    static Font* Construct(Font* font) RETAIL(FUN_001acde8);
    void Destroy(u32 flags) RETAIL(FUN_001ace58);
    // Its pages' materials and textures let go of
    void ReleasePages() RETAIL(ClearFontTexMat);
    // Read from the archive's PSF file of the name (".psf" added): the pages (a texture and a material each, the ones of their IDs
    // the tables have already or read from it), the glyphs and their first character
    void Read(const char* name) RETAIL(ReadFont);
    void Begin() RETAIL(FUN_001acf30);
    void Draw(const TextItem* item, TextPackets* packets) RETAIL(FUN_001acf50);
    void End(TextPackets* packets) RETAIL(FUN_001ab090);
};
CHECK_SIZE(Font, 0x40);

extern "C"
{
    extern const GccVTableEntry g_FontVTable[] RETAIL(Font_Methods);
    extern const GccVTableEntry g_FontBaseVTable[] RETAIL(D_002F6F80);
    // What a font's name gets to be its file's
    extern const char g_FontExtension[] RETAIL(D_00309C28);
    // The glyphs' scale that draws the font's "a" at the font's size (the renderer's frame size in pixels)
    void FontScale(const Font* font, Vector2* scale) RETAIL(FUN_001aa048);
    // The text laid out with the glyphs scaled
    void RenderText(const Font* font, const char* text, const Vector2* scale, TextLayout* layout);
}
