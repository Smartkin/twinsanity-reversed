#include "game/commands.h"

#include "game/math.h"
#include "game/objectnode.h"
#include "game/progress.h"
#include "game/scripttokens.h"

// More of the commands' development tools parsers (game/commandtokens.cpp): each token's kind names an argument, its value goes
// into the command's field. The retail game never calls them

namespace
{
constexpr f32 Epsilon = 0x1.a36e2ep-15f;
constexpr f32 DegreesToAngle = 0x1.6C16C2p+7f;
constexpr f32 DegreesToRadians = 0x1.1df46cp-6f;

bool IsNearlyZero(const f32* vector)
{
    return __builtin_fabsf(vector[0]) <= Epsilon && __builtin_fabsf(vector[1]) <= Epsilon &&
           __builtin_fabsf(vector[2]) <= Epsilon;
}
}

void SetStateCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        if (token->kind == 0x61)
        {
            value1 = (value1 & ~0xFFu) | TokenIsZero(token);
        }

        reader.Next();
    }
}

void SetKeyPathByte43Command::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        if (token->kind == 0xFFFF && token->type == 1)
        {
            value = (value & ~0xFFu) | (token->value & 0xFF);
        }

        reader.Next();
    }
}

void SetCharacterAnalogCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        if (token->kind == 0xD5)
        {
            value = token->Float();
        }

        reader.Next();
    }
}

void AddCharacterAnalogCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        if (token->kind == 0xD5)
        {
            delta = token->Float();
        }

        reader.Next();
    }
}

void ShadowToggleCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        if (token->kind == 0x122)
        {
            slot = (slot & ~0xFFu) | (token->value & 0xFF);
        }

        reader.Next();
    }
}

void SetNode10SlotCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        if (token->kind == 0x122)
        {
            slot = (slot & ~0xFFu) | (token->value & 0xFF);
        }

        reader.Next();
    }
}

void StartQueuedVideoCommand::ParseTokens(const ScriptTokenList*)
{
}

void StopVideoCommand::ParseTokens(const ScriptTokenList*)
{
}

void StopSoundCommand::ParseTokens(const ScriptTokenList*)
{
}

void EndContextMusicCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        if (token->kind == 0x14)
        {
            time = token->Float();
        }

        reader.Next();
    }
}

void SetSoundParamsCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->kind)
        {
        case 0xB:
        {
            // The volume is a float of at most 1 (commands.h has an integer)
            f32 loudness = token->Float();
            if (1.0f < loudness)
            {
                loudness = 1.0f;
            }

            volume = __builtin_bit_cast(s32, loudness);
            set.raw |= 0x2;
            break;
        }
        case 0x98:
            pitch = token->Float();
            set.raw |= 0x1;
            break;
        default:
            break;
        }

        reader.Next();
    }
}

void SetCollisionsCommand::ParseTokens(const ScriptTokenList* tokens)
{
    // value2 to value4 and value13 are floats (commands.h has integers)
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->kind)
        {
        case 0x82:
            flags |= 0x80000;
            value2 = static_cast<s32>(token->value);
            break;
        case 0x83:
            flags |= 0x80000;
            value3 = static_cast<s32>(token->value);
            break;
        case 0x84:
            flags |= 0x80000;
            value4 = static_cast<s32>(token->value);
            break;
        case 0x6A:
            ParseTaggedValueRecord(token, &radius);
            flags |= 0x800;
            break;
        case 0x66:
            // The keywords pick the value of bits 0-3, whatever the token's type
            switch (token->value)
            {
            case 0x1:
                flags = (flags | 0x100) & ~0xFu;
                break;
            case 0x30:
                flags = ((flags | 0x100) & ~0xFu) | 0x1;
                break;
            case 0x8D:
                flags = ((flags | 0x100) & ~0xFu) | 0x6;
                break;
            case 0x31:
                flags = ((flags | 0x100) & ~0xFu) | 0x9;
                break;
            case 0x8A:
                flags = ((flags | 0x100) & ~0xFu) | 0x5;
                break;
            case 0x32:
                flags = ((flags | 0x100) & ~0xFu) | 0xA;
                break;
            case 0x33:
                flags = ((flags | 0x100) & ~0xFu) | 0xB;
                break;
            case 0x3B:
                flags = ((flags | 0x100) & ~0xFu) | 0x8;
                break;
            default:
                break;
            }

            break;
        case 0x67:
            // The same of bits 4-7, without 0x3B
            switch (token->value)
            {
            case 0x1:
                flags = (flags | 0x200) & ~0xF0u;
                break;
            case 0x30:
                flags = ((flags | 0x200) & ~0xF0u) | 0x10;
                break;
            case 0x8D:
                flags = ((flags | 0x200) & ~0xF0u) | 0x60;
                break;
            case 0x31:
                flags = ((flags | 0x200) & ~0xF0u) | 0x90;
                break;
            case 0x8A:
                flags = ((flags | 0x200) & ~0xF0u) | 0x50;
                break;
            case 0x32:
                flags = ((flags | 0x200) & ~0xF0u) | 0xA0;
                break;
            case 0x33:
                flags = ((flags | 0x200) & ~0xF0u) | 0xB0;
                break;
            default:
                break;
            }

            break;
        case 0x3F:
            flags |= 0x2000;
            value8 = token->Float();
            break;
        case 0x68:
            height = token->Float();
            flags |= 0x400;
            break;
        case 0x79:
            flags |= 0x10000;
            value12 = token->Float();
            break;
        case 0x73:
        case 0x94:
            flags |= 0x4000;
            value10 = token->Float();
            break;
        case 0x74:
            flags |= 0x8000;
            value11 = token->Float();
            break;
        case 0x88:
            flags |= 0x400000;
            value11 = token->Float();
            break;
        case 0x87:
        {
            s32 angle;
            AngleFrom(&angle, token->Float(), AngleDegrees);
            value12 = CosOfAngle(&angle);
            flags |= 0x200000;
            break;
        }
        case 0x8B:
            flags |= 0x800000;
            value13 = static_cast<s32>(token->value);
            break;
        case 0x8F:
            flags |= 0x1000000;
            value9 = token->Float();
            break;
        case 0x90:
            value14 = token->Float();
            break;
        case 0xCE:
            value15 = token->Float();
            break;
        case 0xEF:
            flags |= 0x20000000;
            value15 = token->Float();
            break;
        case 0xF0:
            flags |= 0x40000000;
            value15 = token->Float();
            break;
        case 0xFFFF:
            if (token->type == 4)
            {
                switch (token->value)
                {
                case 0x3A:
                    flags |= 0x1000;
                    break;
                case 0x3F:
                    flags = (flags & ~0x60000u) | 0x20000;
                    break;
                case 0x40:
                    flags = (flags & ~0x60000u) | 0x40000;
                    break;
                case 0x41:
                    flags &= ~0x60000u;
                    break;
                case 0x43:
                    flags |= 0x100000;
                    break;
                case 0x8E:
                    flags |= 0x2000000;
                    break;
                case 0x17:
                    flags |= 0x4000000;
                    break;
                case 0x34:
                    flags |= 0x8000000;
                    break;
                case 0xC7:
                    flags |= 0x10000000;
                    break;
                case 0xD1:
                    // Falls through into the default (no break in retail)
                    flags |= 0x80000000;
                    [[fallthrough]];
                default:
                    value9 = -1.0f;
                    break;
                }
            }

            break;
        default:
            break;
        }

        reader.Next();
    }
}

void SetLogicalRadiusCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->kind)
        {
        case 0x68:
            value2 = token->Float();
            break;
        case 0xFFFF:
            if (token->type == 4 && token->value == 0x34)
            {
                value1 |= 0x1;
            }

            break;
        default:
            break;
        }

        reader.Next();
    }
}

void SetSurfaceCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->kind)
        {
        case 0x102:
            surface = ((surface & ~0x1FE0000u) | ((token->value & 0xFF) << 17)) & ~0x10000u;
            break;
        case 0x45:
            surface = (surface & ~0xFFFFu) | (token->value & 0xFFFF);
            break;
        case 0xFFFF:
            if (token->type == 4 && token->value == 0x3E)
            {
                surface |= 0x10000;
            }

            break;
        default:
            break;
        }

        reader.Next();
    }
}

void ContinueColliderMotionCommand::ParseTokens(const ScriptTokenList* tokens)
{
    // The velocity and value6 to value8 are floats (commands.h has integers)
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->kind)
        {
        case 0x49:
            value1.raw |= 0x1;
            velX = static_cast<s32>(token->value);
            break;
        case 0x4A:
            value1.raw |= 0x1;
            velY = static_cast<s32>(token->value);
            break;
        case 0x4B:
            value1.raw |= 0x1;
            velZ = static_cast<s32>(token->value);
            break;
        case 0x3F:
            value1.raw |= 0x2;
            value6 = static_cast<s32>(token->value);
            break;
        case 0x73:
            value1.raw |= 0x4;
            value7 = static_cast<s32>(token->value);
            break;
        case 0x74:
            value1.raw |= 0x8;
            value8 = static_cast<s32>(token->value);
            break;
        case 0xFFFF:
            if (token->type == 4 && token->value == 0x42)
            {
                value1.raw |= 0x10;
            }

            break;
        default:
            break;
        }

        reader.Next();
    }
}

void ColliderLaunchNowCommand::ParseTokens(const ScriptTokenList* tokens)
{
    // value8, value10, value12 to value14 and value11 (radians) are floats (commands.h has integers)
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        flags = (flags & ~0xFF000u) | ((TokenDesignator(token, (flags >> 12) & 0xFF) & 0xFF) << 12);
        flags = (flags & ~0xFu) | (TokenSpace(token, flags & 0xF) & 0xF);
        TokenVectorComponent(token, &x);
        switch (token->kind)
        {
        case 0x6:
            flags = (flags & ~0xFF0u) | ((token->value & 0xFF) << 4);
            break;
        case 0x49:
            flags = (flags | 0x100000) & ~0x200000u;
            x = token->Float();
            break;
        case 0x4A:
            y = token->Float();
            flags = (flags | 0x100000) & ~0x200000u;
            break;
        case 0x4B:
            z = token->Float();
            flags = (flags | 0x100000) & ~0x200000u;
            break;
        case 0x3F:
            flags |= 0x400000;
            value8 = static_cast<s32>(token->value);
            break;
        case 0x73:
            flags |= 0x800000;
            value9 = token->Float();
            break;
        case 0x74:
            flags |= 0x1000000;
            value10 = static_cast<s32>(token->value);
            break;
        case 0x75:
            flags |= 0x2000000;
            value11 = __builtin_bit_cast(u32, token->Float() * DegreesToRadians);
            break;
        case 0xC:
            flags |= 0x10000000;
            power = token->Float();
            break;
        case 0xC5:
            flags2 |= 0x8;
            [[fallthrough]];
        case 0x41:
            flags |= 0x20000000;
            power = token->Float();
            break;
        case 0xAA:
            flags2 |= 0x1;
            value12 = static_cast<s32>(token->value);
            break;
        case 0xAB:
            flags2 |= 0x2;
            value13 = static_cast<s32>(token->value);
            break;
        case 0xAC:
            flags2 |= 0x4;
            value14 = static_cast<s32>(token->value);
            break;
        case 0xFFFF:
            if (token->type == 4)
            {
                switch (token->value)
                {
                case 0x42:
                    flags |= 0x8000000;
                    break;
                case 0x43:
                    flags |= 0x4000000;
                    break;
                case 0x8E:
                    flags2 |= 0x10;
                    break;
                default:
                    break;
                }
            }

            break;
        default:
            break;
        }

        reader.Next();
    }

    if ((flags & 0x100000) == 0 && !IsNearlyZero(&x))
    {
        flags |= 0x200000;
    }
}

void LaunchAtTargetCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        target.raw = (target.raw & ~0xFF000) | ((TokenDesignator(token, (target.raw >> 12) & 0xFF) & 0xFF) << 12);
        target.raw = (target.raw & ~0xF) | (TokenSpace(token, target.raw & 0xF) & 0xF);
        TokenVectorComponent(token, &offsetX);
        switch (token->kind)
        {
        case 0x6:
            target.raw = (target.raw & ~0xFF0) | ((token->value & 0xFF) << 4);
            break;
        case 0x49:
            target.raw = (target.raw | 0x100000) & ~0x200000;
            offsetX = token->Float();
            break;
        case 0x4A:
            offsetY = token->Float();
            target.raw = (target.raw | 0x100000) & ~0x200000;
            break;
        case 0x4B:
            offsetZ = token->Float();
            target.raw = (target.raw | 0x100000) & ~0x200000;
            break;
        case 0xC:
            speedOrAngle = token->Float();
            target.raw |= 0x400000;
            break;
        case 0xC5:
            target.raw |= 0x1000000;
            [[fallthrough]];
        case 0x41:
            speedOrAngle = token->Float();
            target.raw |= 0x800000;
            break;
        case 0xA0:
            value7 = token->Float();
            break;
        case 0x9F:
            value8 = token->Float();
            break;
        default:
            break;
        }

        reader.Next();
    }

    if ((target.raw & 0x100000) == 0 && !IsNearlyZero(&offsetX))
    {
        target.raw |= 0x200000;
    }
}

void ApplyImpulseCommand::ParseTokens(const ScriptTokenList* tokens)
{
    Vector4* velocity = reinterpret_cast<Vector4*>(&velX);
    *velocity = g_DefaultBox.min;
    velocity->w = 1.0f;
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->kind)
        {
        case 0x6:
            value1 = (value1 & ~0xFFu) | (token->value & 0xFF);
            break;
        case 0x49:
            value1 |= 0x1000;
            velX = token->Float();
            break;
        case 0x4A:
            value1 |= 0x1000;
            velY = token->Float();
            break;
        case 0x4B:
            value1 |= 0x1000;
            velZ = token->Float();
            break;
        case 0xA0:
            value1 |= 0x800;
            value6 = token->Float();
            break;
        case 0x9F:
            value1 |= 0x800;
            value7 = token->Float();
            break;
        case 0xFFFF:
            if (token->type == 4)
            {
                switch (token->value)
                {
                case 0x1C:
                    value1 = (value1 & ~0xFFu) | DesignatesFocus;
                    break;
                case 0x44:
                    value1 &= ~0x100u;
                    break;
                case 0x45:
                    value1 |= 0x100;
                    break;
                case 0x46:
                    value1 |= 0x200;
                    break;
                case 0x85:
                    value1 |= 0x400;
                    break;
                default:
                    break;
                }
            }

            break;
        default:
            break;
        }

        reader.Next();
    }
}

void SetPhysicsSizesCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->kind)
        {
        case 0xCD:
            size = token->Float();
            break;
        case 0xCC:
            size2 = token->Float();
            break;
        case 0xFFFF:
            if (token->type == 4)
            {
                switch (token->value)
                {
                case 0xA7:
                    mode2.raw = 2;
                    break;
                case 0xA8:
                    mode2.raw = 1;
                    break;
                case 0xA9:
                    mode1 = 0;
                    break;
                case 0xAA:
                    mode1 = 1;
                    break;
                default:
                    break;
                }
            }

            break;
        default:
            break;
        }

        reader.Next();
    }
}

void MoveInstancesInBoxCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->kind)
        {
        case 0x82:
            offsetX = token->Float();
            break;
        case 0x83:
            offsetY = token->Float();
            break;
        case 0x84:
            offsetZ = token->Float();
            break;
        case 0x94:
            value5 = token->Float();
            break;
        case 0x103:
            flags = (flags & ~0xFFu) | 0x1;
            value6 = token->Float();
            break;
        case 0xCD:
            size = token->Float();
            break;
        default:
            break;
        }

        reader.Next();
    }
}

void AttachFocusObjectCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->kind)
        {
        case 0x0:
            flags |= 0x8000;
            offsetX = token->Float();
            break;
        case 0x1:
            flags |= 0x8000;
            offsetY = token->Float();
            break;
        case 0x2:
            flags |= 0x8000;
            offsetZ = token->Float();
            break;
        case 0x3:
            flags |= 0x10000;
            rotX = static_cast<s32>(token->Float() * DegreesToAngle);
            break;
        case 0x4:
            flags |= 0x10000;
            rotY = static_cast<s32>(token->Float() * DegreesToAngle);
            break;
        case 0x5:
            flags |= 0x10000;
            rotZ = static_cast<s32>(token->Float() * DegreesToAngle);
            break;
        case 0x12:
            flags = (flags & ~0x7F8u) | ((token->value & 0xFF) << 3);
            break;
        case 0x72:
            flags = (flags & ~0x1E0000u) | (((token->value - 1) & 0xF) << 17);
            break;
        case 0xFFFF:
            if (token->type == 4)
            {
                switch (token->value)
                {
                case 0x3D:
                    flags &= ~0x1E0000u;
                    break;
                case 0x4D:
                    flags |= 0x400000;
                    break;
                case 0x4F:
                    flags |= 0x8000;
                    break;
                case 0x50:
                    flags = (flags & ~0x7800u) | 0x800;
                    break;
                case 0x51:
                    flags = (flags & ~0x7800u) | 0x1000;
                    break;
                case 0x7B:
                    flags |= 0x200000;
                    break;
                case 0xA0:
                    flags |= 0x800000;
                    break;
                default:
                    break;
                }
            }

            break;
        default:
            break;
        }

        reader.Next();
    }
}

void AttachSpringCommand::ParseTokens(const ScriptTokenList* tokens)
{
    Vector4* offset = reinterpret_cast<Vector4*>(&offsetX);
    *offset = g_DefaultBox.min;
    offset->w = 1.0f;
    Vector4* position = reinterpret_cast<Vector4*>(&x);
    *position = g_DefaultBox.min;
    position->w = 1.0f;
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        offsetX2.raw = (offsetX2.raw & ~0xF) | (TokenSpace(token, offsetX2.raw & 0xF) & 0xF);
        TokenVectorComponent(token, &x);
        switch (token->kind)
        {
        case 0x82:
            offsetX2.raw |= 0x100000;
            offsetX = token->Float();
            break;
        case 0x83:
            offsetX2.raw |= 0x100000;
            offsetY = token->Float();
            break;
        case 0x84:
            offsetX2.raw |= 0x100000;
            offsetZ = token->Float();
            break;
        case 0x89:
            offsetX2.raw = (offsetX2.raw & ~0xFFFF0) | ((token->value & 0xFFFF) << 4);
            break;
        case 0xED:
            offsetX2.raw |= 0x400000;
            value14 = token->Float();
            break;
        case 0xEE:
            value11 = (value11 & ~0xFFu) | (token->value & 0xFF);
            offsetX2.raw |= 0x200000;
            break;
        case 0x12:
            value11 = (value11 & ~0xFF00u) | ((token->value & 0xFF) << 8);
            break;
        case 0x3E:
            value12 = token->Float();
            break;
        case 0x3F:
            value13 = token->Float();
            break;
        case 0xFFFF:
            // Whatever the token's type
            if (token->value == 0xC8)
            {
                offsetX2.raw |= 0x200000;
            }
            else if (token->value == 0xC9)
            {
                offsetX2.raw |= 0x800000;
            }

            break;
        default:
            break;
        }

        reader.Next();
    }
}

void DropAttachedObjectCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->kind)
        {
        case 0x6D:
            message = (message & ~0xFFFFu) | (token->value & 0xFFFF);
            break;
        case 0x12:
            message = (message & ~0xFF0000u) | ((token->value & 0xFF) << 16);
            break;
        case 0xFFFF:
            if (token->type == 4)
            {
                switch (token->value)
                {
                case 0x35:
                    message = (message & ~0x7000000u) | 0x2000000;
                    break;
                case 0x36:
                    message = (message & ~0x7000000u) | 0x1000000;
                    break;
                case 0x37:
                    message = (message & ~0x7000000u) | 0x3000000;
                    break;
                case 0x4E:
                    message |= 0x20000000;
                    break;
                default:
                    break;
                }
            }

            break;
        default:
            break;
        }

        reader.Next();
    }
}

void ReleaseAgentRef2Command::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->kind)
        {
        case 0xFFFF:
            if (token->type == 4 && token->value == 0x4E)
            {
                event |= 0x10000;
            }

            // Every keyword falls through into the event (no break in retail)
            [[fallthrough]];
        case 0x6D:
            event = (event & ~0xFFFFu) | (token->value & 0xFFFF);
            break;
        default:
            break;
        }

        reader.Next();
    }
}

void ThrowAttachedObjectCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    ParseTaggedValueTokens(tokens, &angleValue);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        value1 = (value1 & ~0xFF00000u) | ((TokenDesignator(token, (value1 >> 20) & 0xFF) & 0xFF) << 20);
        value1 = (value1 & ~0xF00u) | ((TokenSpace(token, (value1 >> 8) & 0xF) & 0xF) << 8);
        TokenVectorComponent(token, &x);
        switch (token->kind)
        {
        case 0x12:
            value1 = (value1 & ~0xFFu) | (token->value & 0xFF);
            break;
        case 0x6:
            value1 = (value1 & ~0xFF000u) | ((token->value & 0xFF) << 12);
            break;
        case 0x58:
            value1 |= 0x10000000;
            if ((value1 & 0xFF00000) != 0xFF00000)
            {
                value1 = (value1 & ~0xFF00000u) | (((((value1 >> 20) & 0xFF) + 1) & 0xFF) << 20);
            }

            break;
        case 0x3C:
            value1 |= 0x40000000;
            value8 = token->Float();
            break;
        case 0x6A:
            ParseTaggedValueRecord(token, &radius);
            break;
        case 0xFFFF:
            if (token->type == 4 && token->value == 0x4E)
            {
                value1 |= 0x80000000;
            }

            break;
        default:
            break;
        }

        reader.Next();
    }

    if (!IsNearlyZero(&x))
    {
        value1 |= 0x20000000;
    }
}

void LaunchAgentRef2Command::ParseTokens(const ScriptTokenList* tokens)
{
    MotionBlock* motion = reinterpret_cast<MotionBlock*>(&motion0);
    ParseMotionBlockTokens(tokens, motion);
    ParseTaggedValueTokens(tokens, &timeOrDistance);
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        launch = (launch & ~0xFF00000u) | ((TokenDesignator(token, (launch >> 20) & 0xFF) & 0xFF) << 20);
        launch = (launch & ~0xF00u) | ((TokenSpace(token, (launch >> 8) & 0xF) & 0xF) << 8);
        TokenVectorComponent(token, &offsetX);
        switch (token->kind)
        {
        case 0x12:
            launch = (launch & ~0xFFu) | (token->value & 0xFF);
            break;
        case 0x6:
            launch = (launch & ~0xFF000u) | ((token->value & 0xFF) << 12);
            break;
        case 0x58:
            launchFlags.raw |= 0x1;
            if ((launch & 0xFF00000) != 0xFF00000)
            {
                launch = (launch & ~0xFF00000u) | (((((launch >> 20) & 0xFF) + 1) & 0xFF) << 20);
            }

            break;
        case 0x3C:
            launchFlags.raw |= 0x4;
            scaleFactor = token->Float();
            break;
        case 0xA0:
            height = token->Float();
            break;
        case 0x9F:
            value44 = token->Float();
            break;
        case 0xFFFF:
            if (token->type == 4)
            {
                switch (token->value)
                {
                case 0x4E:
                    launchFlags.raw |= 0x8;
                    break;
                case 0x76:
                    launchFlags.raw |= 0x40;
                    break;
                case 0x79:
                    launchFlags.raw |= 0x80;
                    break;
                default:
                    break;
                }
            }

            break;
        default:
            break;
        }

        reader.Next();
    }

    if (!IsNearlyZero(&offsetX))
    {
        launchFlags.raw |= 0x2;
    }

    motion->flags |= MotionBlock::FollowedWhenTouched;
}

void UnsupportAboveCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    Vector4* position = reinterpret_cast<Vector4*>(&x);
    *position = g_DefaultBox.min;
    position->w = 1.0f;
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->kind)
        {
        case 0x0:
            x = token->Float();
            break;
        case 0x1:
            y = token->Float();
            break;
        case 0x2:
            z = token->Float();
            break;
        case 0x6:
            value1 = (value1 & ~0xFFu) | (token->value & 0xFF);
            break;
        case 0x7:
            value1 = (value1 & ~0xFF00u) | (((token->value - 1) & 0xFF) << 8);
            break;
        case 0x58:
            value1 |= 0x100000;
            if (((value1 >> 8) & 0xFF) != 0xFF)
            {
                value1 = (value1 & ~0xFF00u) | (((((value1 >> 8) & 0xFF) + 1) & 0xFF) << 8);
            }

            break;
        case 0xFFFF:
            if (token->type == 4)
            {
                switch (token->value)
                {
                case 0x2:
                    value1 &= ~0xF0000u;
                    break;
                case 0x3:
                    value1 = (value1 & ~0xF0000u) | 0x10000;
                    break;
                case 0x4:
                    value1 = (value1 & ~0xF0000u) | 0x20000;
                    break;
                case 0x5:
                    value1 = (value1 & ~0xF0000u) | 0x30000;
                    break;
                case 0x21:
                    value1 = (value1 & ~0xF0000u) | 0x70000;
                    break;
                case 0x24:
                    value1 = (value1 & ~0xFF00u) | (DesignatesCurrentKey << 8);
                    break;
                case 0x25:
                    value1 = (value1 & ~0xFF00u) | (DesignatesNextKey << 8);
                    break;
                default:
                    break;
                }
            }

            break;
        default:
            break;
        }

        reader.Next();
    }

    if (!IsNearlyZero(&x))
    {
        value1 |= 0x200000;
    }
}

void RotateWithLinkedCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->kind)
        {
        case 0xD6:
            degreesPerSecond = token->Float();
            break;
        case 0xFFFF:
            if (token->type == 4)
            {
                // One axis of bits 0-2 and one of bits 3-5
                switch (token->value)
                {
                case 0x99:
                    axes.raw = (axes.raw & ~0x7) | 0x1;
                    break;
                case 0x9A:
                    axes.raw = (axes.raw & ~0x7) | 0x2;
                    break;
                case 0x9B:
                    axes.raw = (axes.raw & ~0x7) | 0x4;
                    break;
                case 0xBF:
                    axes.raw = (axes.raw & ~0x38) | 0x8;
                    break;
                case 0xC0:
                    axes.raw = (axes.raw & ~0x38) | 0x10;
                    break;
                case 0xC1:
                    axes.raw = (axes.raw & ~0x38) | 0x20;
                    break;
                default:
                    break;
                }
            }

            break;
        default:
            break;
        }

        reader.Next();
    }
}

void ForceVolumeControllerCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    x.raw = (x.raw & ~TaggedValue::TypeMask) | TaggedValue::TypeFloat << TaggedValue::TypeShift;
    x.SetFloat(0.0f);
    y.raw = (y.raw & ~TaggedValue::TypeMask) | TaggedValue::TypeFloat << TaggedValue::TypeShift;
    y.SetFloat(0.0f);
    z.raw = (z.raw & ~TaggedValue::TypeMask) | TaggedValue::TypeFloat << TaggedValue::TypeShift;
    z.SetFloat(0.0f);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->kind)
        {
        case 0x0:
            ParseTaggedValueRecord(token, &x);
            break;
        case 0x1:
            ParseTaggedValueRecord(token, &y);
            break;
        case 0x2:
            ParseTaggedValueRecord(token, &z);
            break;
        case 0xFFFF:
            if (token->type == 4)
            {
                // Two bits, keyword 0's and keyword 1's: any other keyword clears both
                value10.raw = (value10.raw & ~0x3) | (token->value == 0 ? 0x1 : 0x0) | (token->value == 1 ? 0x2 : 0x0);
            }

            break;
        default:
            break;
        }

        reader.Next();
    }
}

void SendUserMessageCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        messageTarget = (messageTarget & ~0xFF0000u) | ((TokenDesignator(token, (messageTarget >> 16) & 0xFF) & 0xFF) << 16);
        switch (token->kind)
        {
        case 0x6D:
            messageTarget = (messageTarget & ~0xFFFFu) | (token->value & 0xFFFF);
            break;
        case 0x72:
            messageFlags |= 0x10000;
            messageTarget = (messageTarget & ~0xFF0000u) | (((token->value - 1) & 0xFF) << 16);
            break;
        case 0x10:
            messageFlags = (messageFlags & ~0xFFFFu) | (token->value & 0xFFFF);
            break;
        case 0xFB:
            messageFlags = (messageFlags & ~0x3E000000u) | ((token->value & 0x1F) << 25) | 0x1000000;
            break;
        case 0xFC:
            value1.raw &= ~TaggedValue::TypeMask;
            ParseTaggedValueRecord(token, &value1);
            messageFlags |= 0x1000000;
            break;
        case 0xFA:
            value2.raw &= ~TaggedValue::TypeMask;
            ParseTaggedValueRecord(token, &value2);
            messageFlags |= 0x1000000;
            break;
        case 0xFFFF:
            if (token->type == 4)
            {
                switch (token->value)
                {
                case 0x1C:
                    messageTarget = (messageTarget & ~0xFF0000u) | (DesignatesFocus << 16);
                    break;
                case 0xCE:
                    messageTarget = (messageTarget & ~0xFF0000u) | (DesignatesLinkedById << 16);
                    break;
                case 0x3D:
                    messageTarget &= ~0xFF0000u;
                    messageFlags |= 0x10000;
                    break;
                case 0x3E:
                    messageFlags |= 0x10000 | 0x20000;
                    break;
                case 0xA5:
                    messageFlags |= 0x10000 | 0x40000;
                    break;
                case 0xC3:
                    messageFlags |= 0x80000;
                    break;
                case 0xC4:
                    messageFlags |= 0x10000 | 0x100000;
                    break;
                case 0xD4:
                    messageFlags |= 0x40000000;
                    break;
                case 0xD3:
                    messageFlags |= 0x80000000;
                    break;
                default:
                    break;
                }
            }

            break;
        default:
            break;
        }

        reader.Next();
    }
}

void BroadcastUserMessageCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    Vector4* position = reinterpret_cast<Vector4*>(&x);
    *position = g_DefaultBox.min;
    position->w = 1.0f;
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        messageTarget = (messageTarget & ~0xFF000000u) | ((TokenDesignator(token, messageTarget >> 24) & 0xFF) << 24);
        messageTarget = (messageTarget & ~0xF000u) | ((TokenSpace(token, (messageTarget >> 12) & 0xF) & 0xF) << 12);
        TokenVectorComponent(token, &x);
        switch (token->kind)
        {
        case 0x6D:
            messageTarget = (messageTarget & ~0x7FFu) | (token->value & 0x7FF);
            break;
        case 0x6:
            messageTarget = (messageTarget & ~0xFF0000u) | ((token->value & 0xFF) << 16);
            break;
        case 0x10:
            unknown10 = (unknown10 & ~0xFFFFu) | (token->value & 0xFFFF);
            break;
        case 0x65:
            radius = token->Float();
            break;
        case 0xFFFF:
            if (token->type == 4 && token->value == 0x1C)
            {
                messageTarget = (messageTarget & ~0xFF0000u) | (DesignatesFocus << 16);
            }

            break;
        default:
            break;
        }

        reader.Next();
    }

    if (!IsNearlyZero(&x))
    {
        messageTarget |= 0x800;
    }
}

void TriggerInstancesInRangeCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->kind)
        {
        case 0x6D:
            event = (event & ~0xFFFFu) | (token->value & 0xFFFF);
            break;
        case 0xFFFF:
            if (token->type == 4)
            {
                switch (token->value)
                {
                case 0x11F:
                    event &= ~0xFF0000u;
                    break;
                case 0x120:
                    event = (event & ~0xFF0000u) | 0x20000;
                    break;
                case 0x121:
                    event = (event & ~0xFF0000u) | 0x30000;
                    break;
                case 0x122:
                    event = (event & ~0xFF0000u) | 0x10000;
                    break;
                default:
                    break;
                }
            }

            break;
        default:
            break;
        }

        reader.Next();
    }
}

void SetNode150FieldsCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->kind)
        {
        case 0x6D:
            values = (values & ~0xFFFFu) | (token->value & 0xFFFF);
            break;
        case 0xFFFF:
            if (token->type == 4)
            {
                if (token->value == 0)
                {
                    values |= 0x10000;
                }
                else if (token->value == 1)
                {
                    values &= ~0x10000u;
                }
            }

            break;
        default:
            break;
        }

        reader.Next();
    }
}

void ScaleModelNodeCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        if (token->kind == 0xFD)
        {
            value = token->Float();
        }

        reader.Next();
    }
}

void PhysicsSetGravityCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        if (token->kind == 0x6A)
        {
            gravity = token->Float();
        }

        reader.Next();
    }
}

void NotifyInstancesWithinCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        if (token->kind == 0x65)
        {
            ParseTaggedValueRecord(token, &radius);
        }

        reader.Next();
    }
}

void SetContactSpringyCommand::ParseTokens(const ScriptTokenList* tokens)
{
    MotionBlock* motion = reinterpret_cast<MotionBlock*>(&drag);
    ParseMotionBlockTokens(tokens, motion);
    motion->flags |= MotionBlock::FollowedWhenTouched;
}

void AttachMotionBlockCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        if (token->kind == 0x6D)
        {
            id = static_cast<s32>(token->value);
        }

        reader.Next();
    }
}

void MakeNoiseCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->kind)
        {
        case 0x65:
            radius = token->Float();
            break;
        case 0xC2:
            loudness = token->Float();
            break;
        default:
            break;
        }

        reader.Next();
    }
}

void SetParentExecutionValueCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        if (token->kind == 0x6D)
        {
            value = (value & ~0xFFFFu) | (token->value & 0xFFFF);
        }

        reader.Next();
    }
}
