#include "game/commands.h"

#include "game/math.h"
#include "game/objectnode.h"
#include "game/progress.h"
#include "game/scripttokens.h"
#include "game/shadows.h"

// More of the commands' development tools parsers (game/commandtokens.cpp): each token's kind names an argument, its value goes
// into the command's field. The retail game never calls them

namespace
{
// A tagged value made a float or an integer and given the token's value
void ParseTaggedFloat(const ScriptToken* token, TaggedValue* value)
{
    value->raw = (value->raw & ~TaggedValue::TypeMask) | TaggedValue::TypeFloat << TaggedValue::TypeShift;
    ParseTaggedValueRecord(token, value);
}

void ParseTaggedInt(const ScriptToken* token, TaggedValue* value)
{
    value->raw &= ~TaggedValue::TypeMask;
    ParseTaggedValueRecord(token, value);
}

// The wave an axis of a motion block follows that a keyword names (0x10 to 0x13, 0x22: 1 to 5), 0 none
u32 WobbleWave(u32 keyword)
{
    switch (keyword)
    {
    case 0x10:
        return 1;
    case 0x11:
        return 2;
    case 0x12:
        return 3;
    case 0x13:
        return 4;
    case 0x22:
        return 5;
    default:
        return 0;
    }
}

// The perception slot a keyword names (0xAD to 0xAF, 0xB3: 0 to 3), -1 none
s32 PerceptionSlotOf(u32 keyword)
{
    switch (keyword)
    {
    case 0xAD:
        return 0;
    case 0xAE:
        return 1;
    case 0xAF:
        return 2;
    case 0xB3:
        return 3;
    default:
        return -1;
    }
}
}

void SetWobbleCommand::ParseTokens(const ScriptTokenList* tokens)
{
    constexpr f32 TwoPi = 0x1.921fb6p+2f;
    constexpr f32 DegreesToRadians = 0x1.1df46cp-6f;
    constexpr u32 OtherWay = 0x14;
    // The arguments are a motion block (game/objectnode.h), flags28 its flags. What commands.h has as the amplitudes are the
    // axes' angular speeds (2π over the token) and the phases are floats too. flags27 has each axis' wave in 3 bits from bit 3,
    // flags28 a bit for each from bit 3 saying it was given
    auto* speeds = reinterpret_cast<f32*>(&amplitudeX);
    auto* phases = reinterpret_cast<f32*>(&phaseX);
    f32 duration = 0.0f;
    f32 startX = 0.0f;
    f32 startY = 0.0f;
    f32 startZ = 0.0f;
    u32 otherWayX = 0;
    u32 otherWayY = 0;
    u32 otherWayZ = 0;
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->kind)
        {
        case 0xC:
            duration = token->Float();
            break;
        case 0x1D:
        {
            u32 wave = WobbleWave(token->value);
            if (wave != 0)
            {
                flags27 = (flags27 & ~0x38u) | (wave << 3);
                flags28 |= 0x8;
            }

            break;
        }
        case 0x1E:
        {
            u32 wave = WobbleWave(token->value);
            if (wave != 0)
            {
                flags27 = (flags27 & ~0x1C0u) | (wave << 6);
                flags28 |= 0x10;
            }

            break;
        }
        case 0x1F:
        {
            u32 wave = WobbleWave(token->value);
            if (wave != 0)
            {
                flags27 = (flags27 & ~0xE00u) | (wave << 9);
                flags28 |= 0x20;
            }

            break;
        }
        case 0x20:
            rateX = token->Float();
            if (((flags27 >> 3) & 0x7) == 0)
            {
                flags27 = (flags27 & ~0x38u) | (1 << 3);
                flags28 |= 0x8;
            }

            break;
        case 0x21:
            rateY = token->Float();
            if (((flags27 >> 6) & 0x7) == 0)
            {
                flags27 = (flags27 & ~0x1C0u) | (1 << 6);
                flags28 |= 0x10;
            }

            break;
        case 0x22:
            rateZ = token->Float();
            if (((flags27 >> 9) & 0x7) == 0)
            {
                flags27 = (flags27 & ~0xE00u) | (1 << 9);
                flags28 |= 0x20;
            }

            break;
        case 0x23:
            speeds[0] = TwoPi / token->Float();
            break;
        case 0x24:
            speeds[1] = TwoPi / token->Float();
            break;
        case 0x25:
            speeds[2] = TwoPi / token->Float();
            break;
        case 0x26:
            phases[0] = token->Float() * DegreesToRadians;
            break;
        case 0x27:
            phases[1] = token->Float() * DegreesToRadians;
            break;
        case 0x28:
            phases[2] = token->Float() * DegreesToRadians;
            break;
        case 0x29:
            startX = token->Float();
            break;
        case 0x2A:
            startY = token->Float();
            break;
        case 0x2B:
            startZ = token->Float();
            break;
        case 0x2C:
            if (token->value == OtherWay)
            {
                otherWayX = 1;
            }

            break;
        case 0x2D:
            if (token->value == OtherWay)
            {
                otherWayY = 1;
            }

            break;
        case 0x2E:
            if (token->value == OtherWay)
            {
                otherWayZ = 1;
            }

            break;
        case 0x42:
            flags27 |= 0x10000;
            phases[0] = token->Float() * DegreesToRadians;
            break;
        case 0x43:
            flags27 |= 0x20000;
            phases[1] = token->Float() * DegreesToRadians;
            break;
        case 0x44:
            flags27 |= 0x40000;
            phases[2] = token->Float() * DegreesToRadians;
            break;
        case 0x4F:
            keyAndObject = (keyAndObject & ~0xFFu) | (token->value & 0xFF);
            break;
        case 0x54:
            flags27 = (flags27 & ~0x300000u) | ((token->value == 0x1E ? 2u : 1u) << 20);
            break;
        case 0x55:
            flags27 = (flags27 & ~0xC00000u) | ((token->value == 0x1E ? 2u : 1u) << 22);
            break;
        case 0x56:
            flags27 = (flags27 & ~0x3000000u) | ((token->value == 0x1E ? 2u : 1u) << 24);
            break;
        case 0x95:
            if (token->value == 0x1C)
            {
                flags28 = (flags28 & ~0x7u) | 0x1;
            }
            else if (token->value == 0x3D)
            {
                flags28 = (flags28 & ~0x7u) | 0x2;
            }

            break;
        case 0x99:
            value10 = token->Float();
            flags27 |= 0xC000000;
            break;
        case 0xFFFF:
            if (token->type == 4)
            {
                switch (token->value)
                {
                case 0x2:
                    flags27 &= ~0x7u;
                    break;
                case 0x3:
                    flags27 = (flags27 & ~0x7u) | 0x1;
                    break;
                case 0x4:
                    flags27 = (flags27 & ~0x7u) | 0x2;
                    break;
                case 0x7C:
                    flags27 = (flags27 & ~0x7u) | 0x4;
                    break;
                case 0xA1:
                    flags27 = (flags27 & ~0x7u) | 0x5;
                    break;
                case 0x21:
                    flags27 |= 0x7;
                    break;
                case 0x16:
                    flags27 |= 0x1000;
                    break;
                case 0x17:
                    flags27 |= 0x8000;
                    break;
                case 0x1A:
                    flags27 |= 0x80000;
                    break;
                case 0x58:
                    flags27 = (flags27 & ~0xC000000u) | 0x4000000;
                    break;
                case 0x59:
                    flags27 = (flags27 & ~0xC000000u) | 0x8000000;
                    break;
                case 0x6F:
                    flags28 |= 0x40;
                    break;
                case 0x70:
                    flags28 |= 0x80;
                    break;
                case 0x7D:
                    flags28 |= 0x200;
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

    SetUpMotionBlock(reinterpret_cast<MotionBlock*>(&amplitudeX), otherWayX, otherWayY, otherWayZ, duration, startX, startY,
                     startZ);
    flags28 |= MotionBlock::FollowedWhenTouched;
}

void AlterWobblePhaseCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->kind)
        {
        case 0x26:
            ParseTaggedFloat(token, &value1);
            value4.raw |= 0x1;
            break;
        case 0x27:
            ParseTaggedFloat(token, &value2);
            value4.raw |= 0x2;
            break;
        case 0x28:
            ParseTaggedFloat(token, &value3);
            value4.raw |= 0x4;
            break;
        default:
            break;
        }

        reader.Next();
    }
}

void AddMotionAnglesCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->kind)
        {
        case 0x26:
            ParseTaggedFloat(token, &x);
            set.raw |= 0x1;
            break;
        case 0x27:
            ParseTaggedFloat(token, &y);
            set.raw |= 0x2;
            break;
        case 0x28:
            ParseTaggedFloat(token, &z);
            set.raw |= 0x4;
            break;
        default:
            break;
        }

        reader.Next();
    }
}

void SetMotionFloatsCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->kind)
        {
        case 0xDC:
            ParseTaggedFloat(token, &a);
            set.raw |= 0x1;
            break;
        case 0xDD:
            ParseTaggedFloat(token, &b);
            set.raw |= 0x2;
            break;
        case 0xDE:
            ParseTaggedFloat(token, &c);
            set.raw |= 0x4;
            break;
        default:
            break;
        }

        reader.Next();
    }
}

void SetHeadTrackingTargetCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->kind)
        {
        case 0xC2:
            weightValue = token->Float();
            break;
        case 0x6:
            target = (target & ~0xFFu) | (token->value & 0xFF);
            break;
        case 0xFFFF:
            switch (token->value)
            {
            case 0x88:
                target |= 0x100;
                break;
            case 0x75:
                target |= 0x200;
                break;
            case 0x1C:
                target |= 0x400;
                break;
            case 0x2F:
                target |= 0x800;
                break;
            default:
                break;
            }

            break;
        default:
            break;
        }

        reader.Next();
    }
}

void SetPerceptionWeightCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->kind)
        {
        case 0xD5:
            // A float's bits (commands.h has the weight as an integer)
            weightValue = static_cast<s32>(token->value);
            break;
        case 0xFFFF:
        {
            s32 index = PerceptionSlotOf(token->value);
            if (index >= 0)
            {
                slot.raw = index;
            }

            break;
        }
        default:
            break;
        }

        reader.Next();
    }
}

void AddPerceptionWeightCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->kind)
        {
        case 0xD5:
            // A float's bits (commands.h has the weight as an integer)
            value = static_cast<s32>(token->value);
            break;
        case 0xFFFF:
        {
            s32 index = PerceptionSlotOf(token->value);
            if (index >= 0)
            {
                slot.raw = index;
            }

            break;
        }
        default:
            break;
        }

        reader.Next();
    }
}

void PushFromPerceptionCommand::ParseTokens(const ScriptTokenList* tokens)
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
            factor = token->Float();
            break;
        case 0xFFFF:
        {
            s32 index = PerceptionSlotOf(token->value);
            if (index >= 0)
            {
                mode.raw = index;
            }

            break;
        }
        default:
            break;
        }

        reader.Next();
    }
}

void PerceptionOp141Command::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        if (token->kind == 0xFFFF)
        {
            s32 index = PerceptionSlotOf(token->value);
            if (index >= 0)
            {
                slot.raw = index;
            }
        }

        reader.Next();
    }
}

void PerceptionOp142Command::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        if (token->kind == 0xFFFF)
        {
            s32 index = PerceptionSlotOf(token->value);
            if (index >= 0)
            {
                slot.raw = index;
            }
        }

        reader.Next();
    }
}

void SetShadowCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->kind)
        {
        case 0x122:
            shadowSlot = (shadowSlot & ~0xFFu) | (token->value & 0xFF);
            break;
        case 0x123:
            ParseTaggedValueRecord(token, &size);
            break;
        case 0x124:
            ParseTaggedValueRecord(token, &height);
            break;
        case 0x125:
            ParseTaggedValueRecord(token, &size2);
            break;
        default:
            break;
        }

        reader.Next();
    }
}

void SetShadowCircleCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->kind)
        {
        case 0x122:
            meshSlotAndMode = (meshSlotAndMode & ~0xFFu) | (token->value & 0xFF);
            break;
        case 0xFFFF:
            meshSlotAndMode = (meshSlotAndMode & ~0xFF00u) | ((ShadowShapeOfToken(token->value) & 0xFF) << 8);
            break;
        case 0x4F:
            meshSlotAndMode = (meshSlotAndMode & ~0xFF0000u) | ((token->value & 0xFF) << 16);
            break;
        case 0xE6:
            ParseTaggedValueRecord(token, &radius);
            break;
        case 0xE8:
            ParseTaggedValueRecord(token, &radius2);
            break;
        case 0xCD:
            ParseTaggedValueRecord(token, &radius);
            radius2 = radius;
            break;
        case 0x29:
            ParseTaggedValueRecord(token, &offsetX);
            break;
        case 0x2A:
            ParseTaggedValueRecord(token, &offsetY);
            break;
        case 0x2B:
            ParseTaggedValueRecord(token, &offsetZ);
            break;
        default:
            break;
        }

        reader.Next();
    }
}

void SetShadowMeshCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->kind)
        {
        case 0x122:
            meshSlotAndMode = (meshSlotAndMode & ~0xFFu) | (token->value & 0xFF);
            break;
        case 0xFFFF:
            meshSlotAndMode = (meshSlotAndMode & ~0xFF00u) | ((ShadowShapeOfToken(token->value) & 0xFF) << 8);
            break;
        case 0x126:
            meshSlotAndMode = (meshSlotAndMode & ~0xFF0000u) | ((token->value & 0xFF) << 16);
            break;
        case 0x127:
            meshSlotAndMode = (meshSlotAndMode & ~0xFF000000u) | ((token->value & 0xFF) << 24);
            break;
        case 0xCD:
            ParseTaggedValueRecord(token, &size);
            break;
        case 0x29:
            ParseTaggedValueRecord(token, &offsetX);
            break;
        case 0x2A:
            ParseTaggedValueRecord(token, &offsetY);
            break;
        case 0x2B:
            ParseTaggedValueRecord(token, &offsetZ);
            break;
        default:
            break;
        }

        reader.Next();
    }
}

void SetShadowRectangleCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->kind)
        {
        case 0x122:
            slot = (slot & ~0xFFu) | (token->value & 0xFF);
            break;
        case 0xFFFF:
            slot = (slot & ~0xFF00u) | ((ShadowShapeOfToken(token->value) & 0xFF) << 8);
            break;
        case 0xE6:
            ParseTaggedValueRecord(token, &size);
            break;
        case 0xE8:
            ParseTaggedValueRecord(token, &size3);
            break;
        case 0xCD:
            ParseTaggedValueRecord(token, &size);
            size3 = size;
            break;
        case 0x29:
            ParseTaggedValueRecord(token, &offsetX);
            break;
        case 0x2A:
            ParseTaggedValueRecord(token, &offsetY);
            break;
        case 0x2B:
            ParseTaggedValueRecord(token, &offsetZ);
            break;
        default:
            break;
        }

        reader.Next();
    }
}

void DoSoundCommand::ParseTokens(const ScriptTokenList* tokens)
{
    soundSlots[0] = 0xFFFF;
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->kind)
        {
        case 0x16:
        {
            // A list of the slots, its count in the flags (past the eighth, into the volume and on)
            u32 count = flags & SlotCountMask;
            flags = (flags & ~SlotCountMask) | ((count + 1) & SlotCountMask);
            reinterpret_cast<u16*>(&soundSlots)[count] = static_cast<u16>(token->value);
            break;
        }
        case 0x92:
            switch (token->value)
            {
            case 0x67:
                flags &= ~(GroupMask << GroupShift);
                break;
            case 0x68:
                flags = (flags & ~(GroupMask << GroupShift)) | (1 << GroupShift);
                break;
            case 0x6A:
                flags = (flags & ~(GroupMask << GroupShift)) | (2 << GroupShift);
                break;
            case 0x69:
                flags = (flags & ~(GroupMask << GroupShift)) | (3 << GroupShift);
                break;
            default:
                break;
            }

            break;
        case 0xB:
            ParseTaggedFloat(token, &volume);
            flags |= HasVolume;
            break;
        case 0x98:
            flags |= HasPitch;
            pitch = token->Float();
            break;
        case 0x97:
            flags |= RandomPitch;
            pitchRandom = token->Float();
            break;
        case 0x96:
            flags |= RandomVolume;
            volumeRandom = token->Float();
            break;
        case 0xE2:
            flags |= Shakes;
            shakeStrength = token->Float();
            break;
        case 0xE0:
            flags |= Shakes;
            shakeAcross = token->Float();
            break;
        case 0xE1:
            flags |= Shakes;
            shakeUp = token->Float();
            break;
        case 0xE4:
            shakeFalloff = token->Float();
            break;
        case 0xFFFF:
            if (token->type == 4)
            {
                switch (token->value)
                {
                case 0x62:
                    flags |= Unplaced;
                    break;
                case 0x52:
                    flags |= 0x1000;
                    break;
                case 0xC:
                    flags |= Tracked;
                    break;
                case 0xD5:
                    flags &= ~KindMask;
                    break;
                case 0xE7:
                    flags = (flags & ~KindMask) | (1 << KindShift);
                    break;
                case 0xE8:
                    flags = (flags & ~KindMask) | (2 << KindShift);
                    break;
                case 0xF7:
                    flags = (flags & ~KindMask) | (3 << KindShift);
                    break;
                case 0xD7:
                    flags = (flags & ~KindMask) | (4 << KindShift);
                    break;
                case 0xD9:
                    flags = (flags & ~KindMask) | (5 << KindShift);
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

void BeginMusicCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->kind)
        {
        case 0x9A:
            ParseTaggedInt(token, &track);
            break;
        case 0xB:
            volume = token->Float();
            if (1.0f < volume)
            {
                volume = 1.0f;
            }
            else if (volume < 0.0f)
            {
                volume = 0.0f;
            }

            break;
        case 0x14:
            fadeTime = token->Float();
            break;
        case 0x65:
            range = token->Float();
            break;
        case 0xFFFF:
            if (token->type == 4)
            {
                switch (token->value)
                {
                case 0x72:
                    flags &= ~0x7000u;
                    break;
                case 0x71:
                    flags = (flags & ~0x7000u) | (1 << 12);
                    break;
                case 0x126:
                    flags = (flags & ~0x7000u) | (2 << 12);
                    break;
                case 0x11C:
                    flags = (flags & ~0x7000u) | (3 << 12);
                    break;
                case 0x111:
                    flags &= ~0x8000u;
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

void FadeSoundGroupCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->kind)
        {
        case 0x14:
            value = token->Float();
            break;
        case 0xFFFF:
            if (token->type == 4 && token->value == 0x126)
            {
                group.raw = (group.raw & ~0x7) | 0x2;
            }

            break;
        default:
            break;
        }

        reader.Next();
    }
}

void SetSoundCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->kind)
        {
        case 0x2F:
            value2 = token->Float();
            value1 = (value1 & ~0x100u) | ((0.0f < value2 ? 1u : 0u) << 8);
            break;
        case 0x11E:
            value3 = token->Float();
            value1 = (value1 & ~0x200u) | ((0.0f < value3 ? 1u : 0u) << 9);
            break;
        case 0x11F:
            value4 = token->Float();
            break;
        case 0xFFFF:
            if (token->type == 4)
            {
                if (token->value >= 0xFE && token->value <= 0x106)
                {
                    value1 = (value1 & ~0xFFu) | (token->value - 0xFE);
                }
                else if (token->value == 0x10A)
                {
                    value7 |= 0x1;
                }
            }

            break;
        default:
            break;
        }

        reader.Next();
    }
}

void QueueVideoCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->kind)
        {
        case 0x9A:
            ParseTaggedInt(token, &movieId);
            break;
        case 0xFFFF:
            if (token->type == 4 && token->value == 0x111)
            {
                flags &= ~0x1u;
            }

            break;
        default:
            break;
        }

        reader.Next();
    }
}

void SetNodeBytes168Command::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->kind)
        {
        case 0x129:
            if (token->value < 0xFF)
            {
                bytes = (bytes & ~0xFFu) | token->value;
            }

            break;
        case 0x12A:
            if (token->value < 0xFF)
            {
                bytes = (bytes & ~0xFF00u) | (token->value << 8);
            }

            break;
        case 0xFFFF:
            if (token->type == 4 && token->value == 0x114)
            {
                bytes |= 0x10000;
            }

            break;
        default:
            break;
        }

        reader.Next();
    }
}

void SpawnResidentAgentCommand::ParseTokens(const ScriptTokenList* tokens)
{
    constexpr f32 Epsilon = Rounded(5e-05);
    // flags and flags2 are one word of 64 bits, read and written whole
    auto& bits = *reinterpret_cast<u64*>(&flags);
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        bits = (bits & ~(0x1Full << 24)) | (static_cast<u64>(TokenSpace(token, (bits >> 24) & 0x1F) & 0x1F) << 24);
        TokenVectorComponent(token, &offsetX);
        switch (token->kind)
        {
        case 0x6F:
            bits |= 1ull << 47;
            ParseTaggedInt(token, &subtype);
            break;
        case 0x12:
            flags = (flags & ~0xFF0000u) | ((token->value & 0xFF) << 16);
            break;
        case 0x7:
            objectAndKey = (objectAndKey & ~0xFF0000u) | (((token->value - 1) & 0xFF) << 16);
            break;
        case 0x6D:
            flags = (flags & ~0xFFFFu) | (token->value & 0xFFFF);
            break;
        case 0x9B:
            objectAndKey = (objectAndKey & ~0xFF000000u) | ((token->value & 0xFF) << 24);
            break;
        case 0x89:
            objectAndKey = (objectAndKey & ~0xFFFFu) | (token->value & 0xFFFF);
            break;
        case 0x9D:
            bits = (bits & ~(1ull << 39)) | (static_cast<u64>(TokenIsZero(token) & 1) << 39);
            break;
        case 0xFFFF:
            if (token->type == 4)
            {
                switch (token->value)
                {
                case 0x24:
                    objectAndKey = (objectAndKey & ~0xFF0000u) | 0xFE0000;
                    break;
                case 0x25:
                    objectAndKey = (objectAndKey & ~0xFF0000u) | 0xFD0000;
                    break;
                case 0x6E:
                    bits |= 1ull << 29;
                    break;
                case 0x3C:
                    bits |= 1ull << 31;
                    break;
                case 0x6C:
                    bits |= 1ull << 32;
                    break;
                case 0x1D:
                    bits |= 1ull << 33;
                    break;
                case 0x1C:
                    bits |= 1ull << 34;
                    break;
                case 0x6B:
                    bits |= 1ull << 35;
                    break;
                case 0x20:
                    bits |= 1ull << 36;
                    break;
                case 0x73:
                    bits |= 1ull << 37;
                    break;
                case 0x77:
                    bits |= 1ull << 38;
                    break;
                case 0x87:
                    bits |= 1ull << 41;
                    break;
                case 0x79:
                    bits |= 1ull << 42;
                    break;
                case 0x9F:
                    bits |= 1ull << 43;
                    break;
                case 0x9C:
                    bits |= 1ull << 44;
                    break;
                case 0x9D:
                    bits |= 1ull << 45;
                    break;
                case 0xAB:
                    bits |= 1ull << 46;
                    break;
                case 0x7A:
                    bits |= 1ull << 48;
                    break;
                case 0x4D:
                    bits |= 1ull << 49;
                    break;
                case 0x4E:
                    bits |= 1ull << 50;
                    break;
                case 0xCC:
                    bits |= 1ull << 51;
                    break;
                case 0xD0:
                    bits |= 1ull << 52;
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

    if (!(__builtin_fabsf(offsetX) <= Epsilon && __builtin_fabsf(offsetY) <= Epsilon && __builtin_fabsf(offsetZ) <= Epsilon))
    {
        bits |= 1ull << 30;
    }
}

void DestroySpawnedAttachmentCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->kind)
        {
        case 0x12:
            value1 = (value1 & ~0xFFu) | (token->value & 0xFF);
            break;
        case 0xFFFF:
            if (token->type == 4 && token->value == 0x3E)
            {
                value1 |= 0x100;
            }

            break;
        default:
            break;
        }

        reader.Next();
    }
}

void SaveScriptStateCommand::ParseTokens(const ScriptTokenList*)
{
}

void ClearSavedScriptStateCommand::ParseTokens(const ScriptTokenList*)
{
}

void MarkTimeCommand::ParseTokens(const ScriptTokenList*)
{
}

void ClearMarkedTimeCommand::ParseTokens(const ScriptTokenList*)
{
}

void VideoControllerUpdateCommand::ParseTokens(const ScriptTokenList*)
{
}

void VideoControllerOp182Command::ParseTokens(const ScriptTokenList*)
{
}

void RunScriptSlotCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->kind)
        {
        case 0x70:
            slotAndFlags = (slotAndFlags & ~0xFFFFu) | (token->value & 0xFFFF);
            break;
        case 0xCB:
            slotAndFlags = (slotAndFlags & ~0x10000u) | ((token->value & 0x1) << 16);
            break;
        default:
            break;
        }

        reader.Next();
    }
}

void SetNodeByte8cCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        if (token->kind == 0xCF)
        {
            ParseTaggedInt(token, &value);
        }

        reader.Next();
    }
}

void SetGlobalByte30a0e9Command::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        if (token->kind == 0xCF)
        {
            ParseTaggedInt(token, &value);
        }

        reader.Next();
    }
}

void ClearThreatsCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        if (token->kind == 0x5E)
        {
            designator = (designator & ~0xFFu) | (token->value == 0 ? 1u : 0u);
        }

        reader.Next();
    }
}

void SetFocusPositionToAgentCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->kind)
        {
        case 0xC7:
            targets = (targets & ~0xFFu) | (TokenDesignator(token, targets & 0xFF) & 0xFF);
            break;
        case 0xC8:
            targets = (targets & ~0xFF00u) | ((TokenDesignator(token, (targets >> 8) & 0xFF) & 0xFF) << 8);
            break;
        case 0xEC:
            targets = (targets & ~0xFF0000u) | ((TokenDesignator(token, (targets >> 16) & 0xFF) & 0xFF) << 16);
            break;
        default:
            break;
        }

        reader.Next();
    }
}

void QueueObjectVideoCommand::ParseTokens(const ScriptTokenList* tokens)
{
    constexpr f32 Smallest = Rounded(0.01);
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->kind)
        {
        case 0x11A:
            value = static_cast<s32>(token->value);
            break;
        case 0x59:
        {
            // A float (in the tagged value), at least 0.01. Retail bug: a value above 1 is made -1, which the minimum then makes
            // 0.01 (probably meant to be 1)
            f32 number = token->Float();
            if (1.0f < number)
            {
                number = -1.0f;
            }

            if (number < Smallest)
            {
                number = Smallest;
            }

            value2.raw = __builtin_bit_cast(s32, number);
            break;
        }
        default:
            break;
        }

        reader.Next();
    }
}

void SetTargetOwnerToSelfCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        target.raw = (target.raw & ~0xFF) | (TokenDesignator(token, target.raw & 0xFF) & 0xFF);
        reader.Next();
    }
}

void SetCollisionBoxSizeCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->kind)
        {
        case 0xE6:
            x = token->Float();
            break;
        case 0xE7:
            y = token->Float();
            break;
        case 0xE8:
            z = token->Float();
            break;
        default:
            break;
        }

        reader.Next();
    }
}

void ControllerRumbleCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        if (token->kind == 0x137)
        {
            value1 = token->Float();
        }

        reader.Next();
    }
}

void ClearAnimationCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->kind)
        {
        case 0xC:
            ParseTaggedValueRecord(token, &value2);
            break;
        case 0x4F:
            value1 = (value1 & ~0xFFu) | (token->value & 0xFF);
            break;
        default:
            break;
        }

        reader.Next();
    }
}

void OffsetFocusPositionCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        TokenVectorComponent(reader.Current(), &x);
        reader.Next();
    }
}

void SetFocusPositionBesidePlayerCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        if (token->kind == 0x0)
        {
            distance = token->Float();
        }

        reader.Next();
    }
}

void RequestAttachmentFocusCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        if (token->kind == 0x12)
        {
            value1 = (value1 & ~0xFFu) | (token->value & 0xFF);
        }

        reader.Next();
    }
}

void SetFocusPositionAtAngleCommand::ParseTokens(const ScriptTokenList* tokens)
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
            distance = token->Float();
            break;
        case 0x132:
            angleValue = token->Float();
            break;
        default:
            break;
        }

        reader.Next();
    }
}

void NowMoveForwardsCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ParseTaggedValueTokens(tokens, &distance);
}

void NowMoveBackwardsCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ParseTaggedValueTokens(tokens, &distance);
}

void NowStrafeLeftCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ParseTaggedValueTokens(tokens, &distance);
}

void NowStrafeRightCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ParseTaggedValueTokens(tokens, &distance);
}

void NowTurnLeftCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ParseTaggedValueTokens(tokens, &angleValue);
}

void NowTurnRightCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ParseTaggedValueTokens(tokens, &angleValue);
}

void TriggerLinkedObjectsCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        if (token->kind == 0xFFFF && token->value == 0x85)
        {
            value1 |= 0x1;
        }

        reader.Next();
    }
}

void NextLinkedObjectInListCommand::ParseTokens(const ScriptTokenList* tokens)
{
    // A list of bytes from links1 on, counted in count's first byte. Retail bug: the seventeenth is written over the count (the
    // list goes on from its value) and the bytes can land past the command
    auto* links = reinterpret_cast<u8*>(&links1);
    auto& linkCount = *reinterpret_cast<u8*>(&count);
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        if (token->kind == 0xFFFF)
        {
            links[linkCount] = static_cast<u8>(token->value);
            linkCount = linkCount + 1;
        }

        reader.Next();
    }
}

void SetLinkedObjectIndexCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        if (token->kind == 0xCF)
        {
            number = static_cast<s32>(token->value);
        }

        reader.Next();
    }
}
