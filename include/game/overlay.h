#pragma once

#include "abi.h"
#include "common.h"
#include "game/array.h"
#include "game/controllers.h"
#include "game/font.h"
#include "game/math.h"
#include "game/string.h"

class Shape2D;

// The UI's 2D overlay a renderer draws at the end of its frame (after the scene): shapes queued in six layers, drawn in the
// layers' order, then texts queued per font, drawn a font at a time. What's queued is dropped once it's drawn

// A text queued in a font: a text item (its place in pixels of the frame, its scale, colour and alignment) holding a copy of the
// text
struct QueuedText
{
    Vector2 position;
    Vector2 scale;
    u32 colour;
    TextAlignment alignment;
    String text;
};
CHECK_SIZE(QueuedText, 0x24);
CHECK_OFFSET(QueuedText, text, offsetof(TextItem, text));

// The texts queued in a font (the array grows by 10)
struct FontTexts
{
    Font* font;
    PointerArray<QueuedText> texts;
};
CHECK_SIZE(FontTexts, 0x14);

// How a queued shape is drawn: as it is (not placed by its matrix), in its own colours (not in its colour)
union QueuedShapeFlags
{
    u32 value;
    struct
    {
        u32 withoutMatrix : 1;
        u32 ownColours : 1;
        u32 unused2 : 30;
    };

    // The bits' masks, for the flags a shape is queued with
    enum Mask : u32
    {
        WithoutMatrix = 0x1,
        OwnColours = 0x2,
    };
};
CHECK_SIZE(QueuedShapeFlags, 4);

// A shape queued in a layer: drawn placed by its matrix or as it is, in its colour or in its own colours
struct QueuedShape
{
    Matrix4x4 matrix;
    QueuedShapeFlags flags;
    u32 colour;
    Shape2D* shape;
    QueuedShape* next;
};
CHECK_SIZE(QueuedShape, 0x50);

extern "C"
{
    QueuedText* QueuedTextConstruct(QueuedText* text, const Vector2* position, const char* string) RETAIL(FUN_001acaf8);
    void QueuedTextDestroy(QueuedText* text, u32 flags) RETAIL(FUN_001acb60);
    FontTexts* FontTextsConstruct(FontTexts* texts, Font* font) RETAIL(FUN_001acbb0);
    void FontTextsDestroy(FontTexts* texts, u32 flags) RETAIL(FUN_001acbf8);
    // Every text deleted
    void FontTextsClear(FontTexts* texts) RETAIL(FUN_001aa338);
    void FontTextsAppend(FontTexts* texts, QueuedText* text) RETAIL(FUN_001acd10);
    // A renderer's texts: made, destroyed, every font's cleared, a font's found, a font's found or added
    PointerArray<FontTexts>* TextQueueConstruct(PointerArray<FontTexts>* queue) RETAIL(FUN_001ac8f8);
    void TextQueueDestroy(PointerArray<FontTexts>* queue, u32 flags) RETAIL(FUN_001aa108);
    void TextQueueClear(PointerArray<FontTexts>* queue) RETAIL(FUN_001aa1e8);
    FontTexts* TextQueueFind(PointerArray<FontTexts>* queue, Font* font) RETAIL(FUN_001aa280);
    FontTexts* TextQueueGet(PointerArray<FontTexts>* queue, Font* font) RETAIL(FUN_001ac9f8);

    QueuedShape* QueuedShapeConstruct(QueuedShape* queued, Shape2D* shape) RETAIL(FUN_001ab9e0);
    QueuedShape* QueuedShapeConstructColoured(QueuedShape* queued, Shape2D* shape, u32 colour) RETAIL(FUN_001aba38);
    QueuedShape* QueuedShapeConstructPlaced(QueuedShape* queued, Shape2D* shape, const Matrix4x4* matrix) RETAIL(FUN_001aba88);
    QueuedShape* QueuedShapeConstructPlacedColoured(QueuedShape* queued, Shape2D* shape, const Matrix4x4* matrix, u32 colour)
        RETAIL(FUN_001abaf8);
    // The shape and the ones after it in its layer destroyed
    void QueuedShapeDestroy(QueuedShape* queued, u32 flags) RETAIL(FUN_001abb60);
    void QueuedShapeDraw(QueuedShape* queued) RETAIL(FUN_001abbb0);

    // The text queued in the renderer's font, scale, colour and alignment at the place (pixels of the frame). No text or an empty
    // one queues nothing
    void QueueText(Renderer* renderer, const char* text, f32 x, f32 y) RETAIL_N32(FUN_001a0b30);
    // The shape queued in the layer in the renderer's colour, as it is or placed by the matrix
    void QueueShape(Renderer* renderer, Shape2D* shape, u32 layer) RETAIL(FUN_001a0be0);
    void QueuePlacedShape(Renderer* renderer, const Matrix4x4* matrix, Shape2D* shape, u32 layer) RETAIL(FUN_001a0ca0);
    // A font's texts drawn (the controller isn't used)
    void DrawFontTexts(GameRendererController* controller, FontTexts* texts) RETAIL(FUN_0019d8c8);
    // The renderer's overlay drawn and dropped
    void DrawOverlay(Renderer* renderer) RETAIL(FUN_0019b8b0);
}
