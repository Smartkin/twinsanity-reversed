#include "game/overlay.h"

#include "game/colour.h"
#include "game/memory.h"
#include "game/renderer.h"
#include "game/shapes.h"

namespace
{
// A queued text's alignment until it's queued
constexpr u32 DefaultAlignment = TextAlignment::TopLeft;
constexpr u32 FontTextsGrowth = 10;

// The font's vtable functions: the frame's texts begun, a text drawn and the texts finished
constexpr u32 FontBeginSlot = 2;
constexpr u32 FontDrawSlot = 3;
constexpr u32 FontEndSlot = 4;

// The colour table's white: texts start in it, and a renderer's colour leaves shapes in their own colours with it
bool IsDefaultColour(u32 colour)
{
    return colour == g_Colours[ColourWhite];
}

void AppendShape(Renderer* renderer, QueuedShape* queued, u32 layer)
{
    if (renderer->layers[layer] == nullptr)
    {
        renderer->layers[layer] = queued;
    }
    else
    {
        renderer->layerEnds[layer]->next = queued;
    }

    renderer->layerEnds[layer] = queued;
}

QueuedShape* NewQueuedShape()
{
    return static_cast<QueuedShape*>(MemoryAllocate(sizeof(QueuedShape)));
}
}

extern "C"
{
    QueuedText* QueuedTextConstruct(QueuedText* text, const Vector2* position, const char* string)
    {
        CopyVector2(&text->position, position);
        GetColor(&text->colour, ColourWhite);
        text->alignment.value = DefaultAlignment;
        StringConstruct(&text->text, string);
        text->scale.y = 1.0f;
        text->scale.x = 1.0f;
        return text;
    }

    void QueuedTextDestroy(QueuedText* text, u32 flags)
    {
        StringDestroy(&text->text);
        if ((flags & FreeAfterDestroy) != 0)
        {
            MemoryDeallocate2_(text);
        }
    }

    FontTexts* FontTextsConstruct(FontTexts* texts, Font* font)
    {
        texts->font = font;
        texts->texts.growth = FontTextsGrowth;
        texts->texts.capacity = FontTextsGrowth;
        texts->texts.count = 0;
        texts->texts.data = static_cast<QueuedText**>(MemoryAllocate2(FontTextsGrowth * sizeof(QueuedText*)));
        return texts;
    }

    void FontTextsDestroy(FontTexts* texts, u32 flags)
    {
        FontTextsClear(texts);
        if (texts->texts.data != nullptr)
        {
            MemoryDeallocate_(texts->texts.data);
        }

        if ((flags & FreeAfterDestroy) != 0)
        {
            MemoryDeallocate2_(texts);
        }
    }

    void FontTextsClear(FontTexts* texts)
    {
        for (u32 i = 0; i < texts->texts.count; i++)
        {
            QueuedText* text = texts->texts.data[i];
            if (text != nullptr)
            {
                StringDestroy(&text->text);
                MemoryDeallocate2_(text);
            }
        }

        texts->texts.count = 0;
    }

    void FontTextsAppend(FontTexts* texts, QueuedText* text)
    {
        texts->texts.Append(text);
    }

    // Room for one font's texts, growing by one
    PointerArray<FontTexts>* TextQueueConstruct(PointerArray<FontTexts>* queue)
    {
        queue->count = 0;
        queue->growth = 1;
        queue->capacity = 1;
        queue->data = static_cast<FontTexts**>(MemoryAllocate2(sizeof(FontTexts*)));
        return queue;
    }

    void TextQueueDestroy(PointerArray<FontTexts>* queue, u32 flags)
    {
        for (u32 i = 0; i < queue->count; i++)
        {
            if (queue->data[i] != nullptr)
            {
                FontTextsDestroy(queue->data[i], DestroyAndFree);
            }
        }

        if (queue->data != nullptr)
        {
            MemoryDeallocate_(queue->data);
        }

        if ((flags & FreeAfterDestroy) != 0)
        {
            MemoryDeallocate2_(queue);
        }
    }

    void TextQueueClear(PointerArray<FontTexts>* queue)
    {
        for (u32 i = 0; i < queue->count; i++)
        {
            FontTextsClear(queue->data[i]);
        }
    }

    FontTexts* TextQueueFind(PointerArray<FontTexts>* queue, Font* font)
    {
        for (u32 i = 0; i < queue->count; i++)
        {
            if (queue->data[i]->font == font)
            {
                return queue->data[i];
            }
        }

        return nullptr;
    }

    FontTexts* TextQueueGet(PointerArray<FontTexts>* queue, Font* font)
    {
        FontTexts* texts = TextQueueFind(queue, font);
        if (texts != nullptr)
        {
            return texts;
        }

        texts = FontTextsConstruct(static_cast<FontTexts*>(MemoryAllocate(sizeof(FontTexts))), font);
        queue->Append(texts);
        return texts;
    }

    QueuedShape* QueuedShapeConstruct(QueuedShape* queued, Shape2D* shape)
    {
        queued->shape = shape;
        queued->next = nullptr;
        queued->flags.value = QueuedShapeFlags::WithoutMatrix | QueuedShapeFlags::OwnColours;
        return queued;
    }

    QueuedShape* QueuedShapeConstructColoured(QueuedShape* queued, Shape2D* shape, u32 colour)
    {
        queued->colour = colour;
        queued->shape = shape;
        queued->next = nullptr;
        queued->flags.value = QueuedShapeFlags::WithoutMatrix;
        return queued;
    }

    QueuedShape* QueuedShapeConstructPlaced(QueuedShape* queued, Shape2D* shape, const Matrix4x4* matrix)
    {
        queued->matrix = *matrix;
        queued->shape = shape;
        queued->next = nullptr;
        queued->flags.value = QueuedShapeFlags::OwnColours;
        return queued;
    }

    QueuedShape* QueuedShapeConstructPlacedColoured(QueuedShape* queued, Shape2D* shape, const Matrix4x4* matrix, u32 colour)
    {
        queued->matrix = *matrix;
        queued->shape = shape;
        queued->next = nullptr;
        queued->colour = colour;
        queued->flags.value = 0;
        return queued;
    }

    void QueuedShapeDestroy(QueuedShape* queued, u32 flags)
    {
        if (queued->next != nullptr)
        {
            QueuedShapeDestroy(queued->next, DestroyAndFree);
        }

        if ((flags & FreeAfterDestroy) != 0)
        {
            MemoryDeallocate2_(queued);
        }
    }

    void QueuedShapeDraw(QueuedShape* queued)
    {
        Shape2D* shape = queued->shape;
        bool ownColours = queued->flags.ownColours != 0;
        if (queued->flags.withoutMatrix != 0)
        {
            if (ownColours)
            {
                CallVirtual<void>(shape, shape->vtable, Shape2D::DrawSlot);
            }
            else
            {
                CallVirtual<void>(shape, shape->vtable, Shape2D::DrawColouredSlot, queued->colour);
            }

            return;
        }

        if (ownColours)
        {
            CallVirtual<void>(shape, shape->vtable, Shape2D::DrawPlacedSlot, &queued->matrix);
        }
        else
        {
            CallVirtual<void>(shape, shape->vtable, Shape2D::DrawPlacedColouredSlot, &queued->matrix, queued->colour);
        }
    }

    void QueueText(Renderer* renderer, const char* string, f32 x, f32 y)
    {
        if (string == nullptr)
        {
            return;
        }

        Vector2 position = {x, y};
        QueuedText* text = QueuedTextConstruct(static_cast<QueuedText*>(MemoryAllocate(sizeof(QueuedText))), &position, string);
        if (text->text.length == 0)
        {
            if (text != nullptr)
            {
                QueuedTextDestroy(text, DestroyAndFree);
            }

            return;
        }

        FontTexts* texts = TextQueueGet(&renderer->texts, renderer->font);
        text->scale.x = renderer->textScale.x;
        text->scale.y = renderer->textScale.y;
        text->colour = renderer->colour;
        text->alignment = renderer->textAlignment;
        FontTextsAppend(texts, text);
    }

    void QueueShape(Renderer* renderer, Shape2D* shape, u32 layer)
    {
        QueuedShape* queued;
        if (IsDefaultColour(renderer->colour))
        {
            queued = QueuedShapeConstruct(NewQueuedShape(), shape);
        }
        else
        {
            queued = QueuedShapeConstructColoured(NewQueuedShape(), shape, renderer->colour);
        }

        AppendShape(renderer, queued, layer);
    }

    void QueuePlacedShape(Renderer* renderer, const Matrix4x4* matrix, Shape2D* shape, u32 layer)
    {
        QueuedShape* queued;
        if (IsDefaultColour(renderer->colour))
        {
            queued = QueuedShapeConstructPlaced(NewQueuedShape(), shape, matrix);
        }
        else
        {
            queued = QueuedShapeConstructPlacedColoured(NewQueuedShape(), shape, matrix, renderer->colour);
        }

        AppendShape(renderer, queued, layer);
    }

    void DrawFontTexts(GameRendererController*, FontTexts* texts)
    {
        if (texts->texts.count == 0)
        {
            return;
        }

        Font* font = texts->font;
        CallVirtual<void>(font, font->vtable, FontBeginSlot);
        TextPackets packets;
        packets.count = 0;
        packets.pages[0] = 0;
        packets.pages[1] = 0;
        packets.pages[2] = 0;
        packets.pages[3] = 0;
        for (u32 i = 0; i < texts->texts.count; i++)
        {
            CallVirtual<void>(font, font->vtable, FontDrawSlot, reinterpret_cast<const TextItem*>(texts->texts.data[i]), &packets);
        }

        CallVirtual<void>(font, font->vtable, FontEndSlot, &packets);
    }

    void DrawOverlay(Renderer* renderer)
    {
        if (renderer->flags.draws == 0)
        {
            return;
        }

        for (u32 layer = 0; layer < Renderer::OverlayLayers; layer++)
        {
            for (QueuedShape* queued = renderer->layers[layer]; queued != nullptr; queued = queued->next)
            {
                QueuedShapeDraw(queued);
            }

            if (renderer->layers[layer] != nullptr)
            {
                QueuedShapeDestroy(renderer->layers[layer], DestroyAndFree);
            }

            renderer->layers[layer] = nullptr;
            renderer->layerEnds[layer] = nullptr;
        }

        for (u32 i = 0; i < renderer->texts.count; i++)
        {
            DrawFontTexts(renderer->controller, renderer->texts.data[i]);
        }

        TextQueueClear(&renderer->texts);
    }
}

EABI_EXPORT(FUN_001a0b30, QueueText);
