#include "game/graphicstables.h"

#include "platform/graphics.h"

GameTexture* TextureFromStream(u32 id, Stream* stream)
{
    TextureTable::Entry* entry = g_TextureTable.Find(id);
    GameTexture* texture = entry != nullptr ? entry->item : nullptr;
    if (texture == nullptr)
    {
        texture = Platform::Graphics::NewTexture(id);
        TakeReference(texture);
        g_TextureTable.Insert(&texture, id);
        Platform::Graphics::ReadTexture(texture, stream);
    }

    return texture;
}

MaterialResource* MaterialFromStream(u32 id, Stream* stream)
{
    MaterialTable::Entry* entry = g_MaterialTable.Find(id);
    MaterialResource* material = entry != nullptr ? entry->item : nullptr;
    if (material == nullptr)
    {
        material = Platform::Graphics::NewMaterial(id);
        TakeReference(material);
        g_MaterialTable.Insert(&material, id);
        Platform::Graphics::ReadMaterial(material, stream);
    }

    return material;
}
