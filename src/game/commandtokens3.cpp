#include "game/commands.h"

#include "game/math.h"
#include "game/objectnode.h"
#include "game/progress.h"
#include "game/properties.h"
#include "game/scripttokens.h"

// More of the commands' development tools parsers (their vtables' slot 2, game/commandtokens.cpp has the others): each token's
// kind names an argument, its value goes into the command's field. The retail game never calls them

void UnlinkTargetCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        target.raw = (target.raw & ~0xFF) | static_cast<s32>(TokenDesignator(token, target.raw & 0xFF) & 0xFF);
        switch (token->kind)
        {
        case 0x72:
            target.raw = (target.raw & ~0xFF) | static_cast<s32>((token->value - 1) & 0xFF);
            target.raw |= 0x200;
            break;
        case 0xFFFF:
            if (token->type == 4)
            {
                if (token->value == 0x3E)
                {
                    target.raw |= 0x100;
                }
                else if (token->value == 0xA5)
                {
                    target.raw |= 0x400;
                }
            }

            break;
        default:
            break;
        }

        reader.Next();
    }
}

void UnlinkFromTargetCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        target = (target & ~0xFFu) | (TokenDesignator(token, target & 0xFF) & 0xFF);
        switch (token->kind)
        {
        case 0x72:
            target = (target & ~0xFFu) | ((token->value - 1) & 0xFF);
            target |= 0x200;
            break;
        case 0xFFFF:
            if (token->type == 4)
            {
                switch (token->value)
                {
                case 0x60:
                    target |= 0x800;
                    break;
                case 0x3E:
                    target |= 0x100;
                    break;
                case 0xA5:
                    target |= 0x400;
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

void RunSlotBehaviourOnLinkedCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        targetAndObject = (targetAndObject & ~0xFFu) | (TokenDesignator(token, targetAndObject & 0xFF) & 0xFF);
        switch (token->kind)
        {
        case 0x70:
            slotAndFlags = (slotAndFlags & ~0xFFFFu) | (token->value & 0xFFFF);
            break;
        case 0x10:
            targetAndObject = (targetAndObject & 0xFFFF) | (token->value << 16);
            break;
        case 0xCB:
            slotAndFlags = (slotAndFlags & ~0x10000u) | ((token->value & 0x1) << 16);
            break;
        case 0xFFFF:
            if (token->type == 4)
            {
                if (token->value == 0xE6)
                {
                    slotAndFlags |= 0x20000;
                }
                else if (token->value == 0xFB)
                {
                    slotAndFlags &= ~0x40000u;
                }
            }

            break;
        default:
            break;
        }

        reader.Next();
    }
}

void StopTargetBehaviourCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        targetAndFlags = (targetAndFlags & ~0xFFu) | (TokenDesignator(token, targetAndFlags & 0xFF) & 0xFF);
        if (token->kind == 0xCB)
        {
            targetAndFlags = (targetAndFlags & ~0x100u) | ((token->value & 0x1) << 8);
        }
        else if (token->kind == 0xFFFF && token->type == 4 && token->value == 0xE6)
        {
            targetAndFlags |= 0x200;
        }

        reader.Next();
    }
}

void ResetTimerCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        target = (target & ~0xFFu) | (TokenDesignator(token, target & 0xFF) & 0xFF);
        if (token->kind == 0xFFFF && token->type == 4 && token->value == 0x127)
        {
            target |= 0x100;
        }

        reader.Next();
    }
}

void DoAnimationCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->kind)
        {
        case 0x59:
            ParseTaggedValueRecord(token, &speed);
            flags |= 0x4000;
            break;
        case 0xC:
            ParseTaggedValueRecord(token, &speed);
            flags |= 0x8000;
            break;
        case 0x11:
        {
            // A list of bytes from animSlots on, its count in flags' bits 0-3 (past the fourth, past the command)
            u32 count = flags & 0xF;
            reinterpret_cast<u8*>(&animSlots)[count] = static_cast<u8>(token->value);
            flags = (flags & ~0xF) | static_cast<s32>((count + 1) & 0xF);
            break;
        }
        case 0x14:
            ParseTaggedValueRecord(token, &blendTime);
            flags |= 0x2000;
            break;
        case 0x4F:
            flags = (flags & ~0xFF0) | static_cast<s32>((token->value & 0xFF) << 4);
            break;
        case 0x11C:
            flags |= 0x20000;
            startPosition = token->Float();
            break;
        case 0x65:
            ParseTaggedValueRecord(token, &speedRandom);
            flags |= 0x10000;
            break;
        case 0x122:
            flags = ((flags | 0x200000) & ~0x3C00000) | static_cast<s32>((token->value & 0xF) << 22);
            break;
        case 0xFFFF:
            if (token->type == 4)
            {
                switch (token->value)
                {
                case 0x27:
                    flags |= 0x80000;
                    break;
                case 0xC:
                    flags |= 0x1000;
                    break;
                case 0xF8:
                    flags |= 0x40000;
                    break;
                case 0xF9:
                    flags |= 0x100000;
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

void ForceAnimationUpdateCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        if (token->kind == 0xFFFF)
        {
            switch (token->value)
            {
            case 0xB6:
                value1.raw = (value1.raw & ~0xF) | 0x1;
                break;
            case 0x6D:
                value1.raw = (value1.raw & ~0xF) | 0x2;
                break;
            case 0xB7:
                value1.raw &= ~0xF;
                break;
            default:
                break;
            }
        }

        reader.Next();
    }
}

void SetCounterCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        counterTarget =
            (counterTarget & ~0xFF0000u) | ((TokenDesignator(token, (counterTarget >> 16) & 0xFF) & 0xFF) << 16);
        switch (token->kind)
        {
        case 0x6E:
        case 0xF5:
            counterTarget = (counterTarget & ~0xFFFFu) | (token->value & 0xFFFF);
            break;
        case 0xB2:
        case 0xF6:
            counterTarget = (counterTarget & ~0xFFFFu) | (token->value & 0xFFFF);
            counterTarget |= 0x1000000;
            break;
        case 0x10F:
            counterTarget |= 0x2000000;
            value.raw &= ~TaggedValue::TypeMask;
            ParseTaggedValueRecord(token, &value);
            break;
        case 0xC3:
            value.raw &= ~TaggedValue::TypeMask;
            ParseTaggedValueRecord(token, &value);
            break;
        case 0xFFFF:
            if (token->type == 1)
            {
                value.raw &= ~TaggedValue::TypeMask;
                ParseTaggedValueRecord(token, &value);
            }
            else if (token->value == 0xE6)
            {
                counterTarget |= 0x4000000;
            }

            break;
        default:
            break;
        }

        reader.Next();
    }
}

void ModifyCounterCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        counterTarget =
            (counterTarget & ~0xFF0000u) | ((TokenDesignator(token, (counterTarget >> 16) & 0xFF) & 0xFF) << 16);
        switch (token->kind)
        {
        case 0xC3:
            delta.raw &= ~TaggedValue::TypeMask;
            ParseTaggedValueRecord(token, &delta);
            break;
        case 0x6E:
        case 0xF5:
            counterTarget = (counterTarget & ~0xFFFFu) | (token->value & 0xFFFF);
            break;
        case 0xB2:
        case 0xF6:
            counterTarget = (counterTarget & ~0xFFFFu) | (token->value & 0xFFFF);
            counterTarget |= 0x1000000;
            break;
        case 0x10:
            unknown3 = (unknown3 & ~0xFFFFu) | (token->value & 0xFFFF);
            break;
        case 0xFFFF:
            if (token->type == 1)
            {
                delta.raw &= ~TaggedValue::TypeMask;
                ParseTaggedValueRecord(token, &delta);
            }
            else if (token->value == 0xE6)
            {
                counterTarget |= 0x2000000;
            }

            break;
        default:
            break;
        }

        reader.Next();
    }
}

void AddToFocusObjectByteCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->kind)
        {
        case 0xF4:
            index = (index & ~0xFFu) | (token->value & 0xFF);
            break;
        case 0xFFFF:
            if (token->type != 1)
            {
                break;
            }

            [[fallthrough]];
        case 0xC3:
            value.raw &= ~TaggedValue::TypeMask;
            ParseTaggedValueRecord(token, &value);
            break;
        default:
            break;
        }

        reader.Next();
    }
}

void SetFocusObjectByteCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->kind)
        {
        case 0xC3:
            value.raw &= ~TaggedValue::TypeMask;
            ParseTaggedValueRecord(token, &value);
            break;
        case 0xB2:
            // Retail has no break here: a source of type 1 is the value as well
            source = static_cast<s32>(token->value);
            [[fallthrough]];
        case 0xFFFF:
            if (token->type == 1)
            {
                value.raw &= ~TaggedValue::TypeMask;
                ParseTaggedValueRecord(token, &value);
            }

            break;
        case 0xF4:
            index = (index & ~0xFFu) | (token->value & 0xFF);
            break;
        default:
            break;
        }

        reader.Next();
    }
}

void SetFocusPositionCommand::ParseTokens(const ScriptTokenList* tokens)
{
    // A position that isn't 0 sets targetFlags' bit 21. Retail keeps targetFlags and angleValue as one 64 bit word of bits
    constexpr f32 Epsilon = 0x1.a36e2ep-15f;
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        targetFlags = (targetFlags & ~0xFF00u) | ((TokenDesignator(token, (targetFlags >> 8) & 0xFF) & 0xFF) << 8);
        targetFlags = (targetFlags & ~0xF0000u) | ((TokenSpace(token, (targetFlags >> 16) & 0xF) & 0xF) << 16);
        TokenVectorComponent(token, &offsetX);
        switch (token->kind)
        {
        case 0xD7:
            value12 = token->Float();
            break;
        case 0x9C:
            scale = token->Float();
            break;
        case 0x3C:
            targetFlags |= 0x400000;
            value8 = token->Float();
            break;
        case 0x6:
            targetFlags = (targetFlags & ~0xFFu) | (token->value & 0xFF);
            break;
        case 0x58:
            // The designator's next (none stays none)
            targetFlags |= 0x100000;
            if (((targetFlags >> 8) & 0xFF) != 0xFF)
            {
                targetFlags += 0x100;
            }

            break;
        case 0xAB:
        case 0x101:
            value16 = __builtin_bit_cast(s32, token->Float());
            break;
        case 0xAA:
            value10 = token->Float();
            break;
        case 0xAC:
            value11 = token->Float();
            break;
        case 0x10E:
            value13 = token->Float();
            break;
        case 0x100:
            value15 = __builtin_bit_cast(s32, token->Float());
            break;
        case 0xFF:
            value14 = __builtin_bit_cast(s32, token->Float());
            break;
        case 0x132:
            angleValue |= 0x2;
            value18 = __builtin_bit_cast(s32, token->Float());
            break;
        case 0x12B:
            keyAndObject = (keyAndObject & ~0xFFu) | (token->value & 0xFF);
            break;
        case 0xFFFF:
            if (token->type == 4)
            {
                switch (token->value)
                {
                case 0xB8:
                    targetFlags |= 0x4000000;
                    break;
                case 0xB9:
                    targetFlags |= 0x8000000;
                    break;
                case 0xBA:
                    targetFlags |= 0x10000000;
                    break;
                case 0xCA:
                    targetFlags |= 0x20000000;
                    break;
                case 0xCB:
                    targetFlags |= 0x40000000;
                    break;
                case 0xCD:
                    targetFlags |= 0x80000000;
                    break;
                case 0xC5:
                    angleValue |= 0x1;
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
        targetFlags |= 0x200000;
    }
}

void AddNoiseToFocusPositionCommand::ParseTokens(const ScriptTokenList* tokens)
{
    amount.raw = (amount.raw & ~TaggedValue::TypeMask) | TaggedValue::TypeFloat << TaggedValue::TypeShift;
    amount.SetFloat(1.0f);
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->kind)
        {
        case 0x9C:
            factorB = token->Float();
            break;
        case 0x65:
            ParseTaggedValueRecord(token, &amount);
            break;
        case 0xF1:
            factorA = token->Float();
            break;
        case 0xF2:
            factorC = token->Float();
            break;
        default:
            break;
        }

        reader.Next();
    }
}

void SetFocusToAgentCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        if (token->kind == 0x6)
        {
            targetAndSlot = (targetAndSlot & ~0xFFu) | (token->value & 0xFF);
        }
        else if (token->kind == 0xFFFF)
        {
            switch (token->value)
            {
            case 0x75:
                targetAndSlot = (targetAndSlot & ~0xFFu) | 0xF7;
                break;
            case 0x1C:
                targetAndSlot = (targetAndSlot & ~0xFFu) | 0xFB;
                break;
            case 0x74:
                targetAndSlot = (targetAndSlot & ~0xFFu) | 0xF8;
                break;
            case 0xCE:
                targetAndSlot = (targetAndSlot & ~0xFFu) | 0xDF;
                break;
            case 0x11D:
                targetAndSlot = (targetAndSlot & ~0xFFu) | 0xDE;
                break;
            default:
                break;
            }
        }

        reader.Next();
    }
}

void SetFocusToKeyCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->kind)
        {
        case 0x58:
            keyAndFlags |= 0x100;
            break;
        case 0x7:
            keyAndFlags = (keyAndFlags & ~0xFFu) | ((token->value - 1) & 0xFF);
            break;
        case 0xFFFF:
            switch (token->value)
            {
            case 0x25:
                keyAndFlags = (keyAndFlags & ~0xFFu) | 0xFD;
                break;
            case 0x24:
                keyAndFlags = (keyAndFlags & ~0xFFu) | 0xFE;
                break;
            case 0xC5:
                keyAndFlags |= 0x1000;
                break;
            case 0xC6:
                keyAndFlags |= 0x2000;
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

void SetFocusToLinkedObjectCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        target = (target & ~0xFFu) | (TokenDesignator(token, target & 0xFF) & 0xFF);
        switch (token->kind)
        {
        case 0x72:
            // Counted from 1, the first for anything below
            target = (target & ~0xFFu) | (static_cast<s32>(token->value) > 0 ? (token->value - 1) & 0xFF : 0);
            target |= 0x100;
            break;
        case 0xFFFF:
            if (token->type == 4)
            {
                switch (token->value)
                {
                case 0xC4:
                    target |= 0x8100;
                    break;
                case 0x3D:
                    target &= ~0xFFu;
                    target |= 0x100;
                    break;
                case 0xA5:
                    target |= 0x300;
                    break;
                case 0xC5:
                    target |= 0x400;
                    break;
                case 0xC6:
                    target |= 0x800;
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

void SetFocusPositionAlongCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->kind)
        {
        case 0xC8:
            targets = (targets & ~0xFFu) | (TokenDesignator(token, targets & 0xFF) & 0xFF);
            break;
        case 0x104:
            targets = (targets & ~0xFF00u) | ((TokenDesignator(token, (targets >> 8) & 0xFF) & 0xFF) << 8);
            break;
        case 0x105:
            targets = (targets & ~0xFF0000u) | ((TokenDesignator(token, (targets >> 16) & 0xFF) & 0xFF) << 16);
            break;
        case 0x106:
            distance = token->Float();
            break;
        case 0xFFFF:
            if (token->type == 4 && token->value == 0xCD)
            {
                targets = (targets & ~0xF000000u) | 0x2000000;
            }

            break;
        default:
            break;
        }

        reader.Next();
    }
}

void SetFocusPositionOffsetCommand::ParseTokens(const ScriptTokenList* tokens)
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
            x = token->Float();
            use.raw |= 0x1;
            break;
        case 0x1:
            y = token->Float();
            use.raw |= 0x2;
            break;
        case 0x2:
            z = token->Float();
            use.raw |= 0x4;
            break;
        case 0x82:
            offsetX = token->Float();
            use.raw |= 0x8;
            break;
        case 0x83:
            offsetY = token->Float();
            use.raw |= 0x10;
            break;
        case 0x84:
            offsetZ = token->Float();
            use.raw |= 0x20;
            break;
        case 0xCD:
            distance = token->Float();
            break;
        default:
            break;
        }

        reader.Next();
    }
}

void PositionWarpCommand::ParseTokens(const ScriptTokenList* tokens)
{
    // A position that isn't 0 sets targetAndSpace's bit 21
    constexpr f32 Epsilon = 0x1.a36e2ep-15f;
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    targetAndSpace = ((targetAndSpace & ~0xF0000u) | 0xFFFF) & ~0x100000u & ~0x200000u;
    *reinterpret_cast<Vector4*>(&x) = g_DefaultBox.min;
    w = 1.0f;
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        targetAndSpace =
            (targetAndSpace & ~0xFF00u) | ((TokenDesignator(token, (targetAndSpace >> 8) & 0xFF) & 0xFF) << 8);
        switch (token->kind)
        {
        case 0x58:
            // The designator's next (none stays none)
            targetAndSpace |= 0x100000;
            if (((targetAndSpace >> 8) & 0xFF) != 0xFF)
            {
                targetAndSpace += 0x100;
            }

            // Retail has no break here: the token's value goes on into the low byte
            [[fallthrough]];
        case 0x6:
            targetAndSpace = (targetAndSpace & ~0xFFu) | (token->value & 0xFF);
            break;
        case 0x0:
            x = token->Float();
            break;
        case 0x1:
            y = token->Float();
            break;
        case 0x2:
            z = token->Float();
            break;
        case 0x7:
            targetAndSpace = (targetAndSpace & ~0xFF00u) | (((token->value - 1) & 0xFF) << 8);
            break;
        case 0xFFFF:
            if (token->type == 4)
            {
                switch (token->value)
                {
                case 0x2:
                    targetAndSpace &= ~0xF0000u;
                    break;
                case 0x3:
                    targetAndSpace = (targetAndSpace & ~0xF0000u) | 0x10000;
                    break;
                case 0x4:
                    targetAndSpace = (targetAndSpace & ~0xF0000u) | 0x20000;
                    break;
                case 0x5:
                    targetAndSpace = (targetAndSpace & ~0xF0000u) | 0x30000;
                    break;
                case 0x21:
                    targetAndSpace = (targetAndSpace & ~0xF0000u) | 0x70000;
                    break;
                case 0x1B:
                    targetAndSpace |= 0x800000;
                    break;
                case 0x113:
                    targetAndSpace |= 0x1000000;
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

    if (!(__builtin_fabsf(x) <= Epsilon && __builtin_fabsf(y) <= Epsilon && __builtin_fabsf(z) <= Epsilon))
    {
        targetAndSpace |= 0x200000;
    }
}

void RotationWarpCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    targetAndSpace = ((targetAndSpace & ~0xF0000u) | 0xFFFF) & ~0x100000u & ~0x200000u & ~0x400000u;
    quatY = 0.0f;
    quatW = 0.0f;
    quatZ = 0.0f;
    quatX = 0.0f;
    // Degrees about x, y and z
    f32 angleX = 0.0f;
    f32 angleY = 0.0f;
    f32 angleZ = 0.0f;
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->kind)
        {
        case 0x6:
            targetAndSpace = (targetAndSpace & ~0xFFu) | (token->value & 0xFF);
            break;
        case 0x3:
            angleX = token->Float();
            break;
        case 0x4:
            angleY = token->Float();
            break;
        case 0x5:
            angleZ = token->Float();
            break;
        case 0x58:
            targetAndSpace |= 0x100000;
            if (((targetAndSpace >> 8) & 0xFF) != 0xFF)
            {
                targetAndSpace += 0x100;
            }

            break;
        case 0x7:
            targetAndSpace = (targetAndSpace & ~0xFF00u) | (((token->value - 1) & 0xFF) << 8);
            break;
        case 0xFFFF:
            if (token->type == 4)
            {
                switch (token->value)
                {
                case 0x2:
                    targetAndSpace &= ~0xF0000u;
                    break;
                case 0x3:
                    targetAndSpace = (targetAndSpace & ~0xF0000u) | 0x10000;
                    break;
                case 0x4:
                    targetAndSpace = (targetAndSpace & ~0xF0000u) | 0x20000;
                    break;
                case 0x5:
                    targetAndSpace = (targetAndSpace & ~0xF0000u) | 0x30000;
                    break;
                case 0x21:
                    targetAndSpace = (targetAndSpace & ~0xF0000u) | 0x70000;
                    break;
                case 0x24:
                    targetAndSpace = (targetAndSpace & ~0xFF00u) | 0xFE00;
                    break;
                case 0x25:
                    targetAndSpace = (targetAndSpace & ~0xFF00u) | 0xFD00;
                    break;
                case 0xE0:
                    targetAndSpace |= 0x400000;
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

    if (angleX != 0.0f || angleY != 0.0f || angleZ != 0.0f)
    {
        targetAndSpace |= 0x200000;
        s32 x;
        s32 y;
        s32 z;
        AngleFrom(&x, angleX, AngleDegrees);
        AngleFrom(&y, angleY, AngleDegrees);
        AngleFrom(&z, angleZ, AngleDegrees);
        GetRotationXYZ(reinterpret_cast<Vector4*>(&quatX), &x, &y, &z);
    }
    else
    {
        quatZ = 0.0f;
        quatY = 0.0f;
        quatX = 0.0f;
        quatW = 1.0f;
    }
}

void SetRotationComponentsCommand::ParseTokens(const ScriptTokenList* tokens)
{
    constexpr f32 DegreesToRadians = 0x1.1df46cp-6f;
    components &= ~0x7u;
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->kind)
        {
        case 0x3:
            x = token->Float() * DegreesToRadians;
            components |= 0x1;
            break;
        case 0x4:
            y = token->Float() * DegreesToRadians;
            components |= 0x2;
            break;
        case 0x5:
            z = token->Float() * DegreesToRadians;
            components |= 0x4;
            break;
        default:
            break;
        }

        reader.Next();
    }
}

void WarpAgentCommand::ParseTokens(const ScriptTokenList* tokens)
{
    // An offset that isn't 0 sets the warp's bit 21
    constexpr f32 Epsilon = 0x1.a36e2ep-15f;
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    warp = ((warp & ~0xF0000u) | 0xFFFF) & ~0x100000u & ~0x200000u;
    *reinterpret_cast<Vector4*>(&offsetX) = g_DefaultBox.min;
    offsetW = 1.0f;
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        warp = (warp & ~0xFF00u) | ((TokenDesignator(token, (warp >> 8) & 0xFF) & 0xFF) << 8);
        switch (token->kind)
        {
        case 0x7:
            warp = (warp & ~0xFF00u) | (((token->value - 1) & 0xFF) << 8);
            break;
        case 0x0:
            offsetX = token->Float();
            break;
        case 0x1:
            offsetY = token->Float();
            break;
        case 0x2:
            offsetZ = token->Float();
            break;
        case 0x6:
            warp = (warp & ~0xFFu) | (token->value & 0xFF);
            break;
        case 0x105:
            source = (source & ~0xFFu) | (TokenDesignator(token, source & 0xFF) & 0xFF);
            break;
        case 0x58:
            // The designator's next (none stays none)
            warp |= 0x100000;
            if (((warp >> 8) & 0xFF) != 0xFF)
            {
                warp += 0x100;
            }

            // Retail has no break here: the token also names the designator in the top byte
            [[fallthrough]];
        case 0x117:
            warp = (warp & 0xFFFFFF) | ((TokenDesignator(token, warp >> 24) & 0xFF) << 24);
            break;
        case 0xFFFF:
            if (token->type == 4)
            {
                switch (token->value)
                {
                case 0x2:
                    warp &= ~0xF0000u;
                    break;
                case 0x3:
                    warp = (warp & ~0xF0000u) | 0x10000;
                    break;
                case 0x4:
                    warp = (warp & ~0xF0000u) | 0x20000;
                    break;
                case 0x5:
                    warp = (warp & ~0xF0000u) | 0x30000;
                    break;
                case 0x21:
                    warp = (warp & ~0xF0000u) | 0x70000;
                    break;
                case 0x1B:
                    warp |= 0x800000;
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
        warp |= 0x200000;
    }
}

void RotateAgentCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    rotate = ((rotate & ~0xF0000u) | 0xFFFF) & ~0x100000u & ~0x200000u & ~0x400000u;
    quatY = 0.0f;
    quatW = 0.0f;
    quatZ = 0.0f;
    quatX = 0.0f;
    // Degrees about x, y and z
    f32 angleX = 0.0f;
    f32 angleY = 0.0f;
    f32 angleZ = 0.0f;
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->kind)
        {
        case 0x7:
            rotate = (rotate & ~0xFF00u) | (((token->value - 1) & 0xFF) << 8);
            break;
        case 0x3:
            angleX = token->Float();
            break;
        case 0x4:
            angleY = token->Float();
            break;
        case 0x5:
            angleZ = token->Float();
            break;
        case 0x6:
            rotate = (rotate & ~0xFFu) | (token->value & 0xFF);
            break;
        case 0x105:
            subject = (subject & ~0xFF00u) | ((TokenDesignator(token, (subject >> 8) & 0xFF) & 0xFF) << 8);
            break;
        case 0x58:
            rotate |= 0x100000;
            if (((rotate >> 8) & 0xFF) != 0xFF)
            {
                rotate += 0x100;
            }

            break;
        case 0x117:
            subject = (subject & ~0xFFu) | (TokenDesignator(token, subject & 0xFF) & 0xFF);
            break;
        case 0xFFFF:
            if (token->type == 4)
            {
                switch (token->value)
                {
                case 0x2:
                    rotate &= ~0xF0000u;
                    break;
                case 0x3:
                    rotate = (rotate & ~0xF0000u) | 0x10000;
                    break;
                case 0x4:
                    rotate = (rotate & ~0xF0000u) | 0x20000;
                    break;
                case 0x5:
                    rotate = (rotate & ~0xF0000u) | 0x30000;
                    break;
                case 0x21:
                    rotate = (rotate & ~0xF0000u) | 0x70000;
                    break;
                case 0x24:
                    rotate = (rotate & ~0xFF00u) | 0xFE00;
                    break;
                case 0x25:
                    rotate = (rotate & ~0xFF00u) | 0xFD00;
                    break;
                case 0xE0:
                    rotate |= 0x400000;
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

    if (angleX != 0.0f || angleY != 0.0f || angleZ != 0.0f)
    {
        rotate |= 0x200000;
        s32 x;
        s32 y;
        s32 z;
        AngleFrom(&x, angleX, AngleDegrees);
        AngleFrom(&y, angleY, AngleDegrees);
        AngleFrom(&z, angleZ, AngleDegrees);
        GetRotationXYZ(reinterpret_cast<Vector4*>(&quatX), &x, &y, &z);
    }
    else
    {
        quatZ = 0.0f;
        quatY = 0.0f;
        quatX = 0.0f;
        quatW = 1.0f;
    }
}

void StrafeTowardsTargetCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->kind)
        {
        case 0x39:
            speed = token->Float();
            break;
        case 0x20:
            maxDistance = token->Float();
            break;
        case 0xFFFF:
            if (token->type == 4)
            {
                switch (token->value)
                {
                case 0x1C:
                    target = (target & ~0xFFu) | 0xFB;
                    break;
                case 0x74:
                    target = (target & ~0xFFu) | 0xF8;
                    break;
                case 0x75:
                    target = (target & ~0xFFu) | 0xF7;
                    break;
                case 0x1D:
                    target = (target & ~0xFFu) | 0xFC;
                    break;
                case 0x3:
                    target = (target & ~0xF00u) | 0x100;
                    break;
                case 0x4:
                    target = (target & ~0xF00u) | 0x200;
                    break;
                case 0x17:
                    target |= 0x1000;
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

void MoveTowardsDesignatorCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->kind)
        {
        case 0x39:
            distance = __builtin_bit_cast(s32, token->Float());
            break;
        case 0xEA:
            value2 = token->Float();
            break;
        case 0xEB:
            value3 = __builtin_bit_cast(s32, token->Float());
            break;
        case 0x40:
            value4 = __builtin_bit_cast(s32, token->Float());
            break;
        case 0xFFFF:
            if (token->type == 4)
            {
                switch (token->value)
                {
                case 0x1C:
                    target = (target & ~0xFFu) | 0xFB;
                    break;
                case 0x1D:
                    target = (target & ~0xFFu) | 0xFC;
                    break;
                case 0x74:
                    target = (target & ~0xFFu) | 0xF8;
                    break;
                case 0x75:
                    target = (target & ~0xFFu) | 0xF7;
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

void AddToLinkedObjectsByteCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->kind)
        {
        case 0xC3:
            value.raw &= ~TaggedValue::TypeMask;
            ParseTaggedValueRecord(token, &value);
            break;
        case 0x10:
            filter = (filter & ~0xFFFFu) | (token->value & 0xFFFF);
            break;
        case 0xF6:
            filter = (filter & 0xFFFF) | (token->value << 16);
            break;
        case 0x72:
            target = (target & ~0xFFu) | ((token->value - 1) & 0xFF);
            break;
        case 0xFFFF:
            if (token->type == 4)
            {
                switch (token->value)
                {
                case 0x3E:
                    target |= 0x100;
                    break;
                case 0xA5:
                    target |= 0x200;
                    break;
                case 0x3D:
                    target |= 0x400;
                    break;
                case 0xC4:
                    target |= 0x800;
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

void ArrangeLinkedObjectsCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->kind)
        {
        case 0x4:
            flags |= 0x100000;
            angleValue = token->Float();
            break;
        case 0x11B:
            flags |= 0x200000;
            value = token->Float();
            break;
        case 0x12E:
            flags = (flags & ~0xFF000u) | ((token->value & 0xFF) << 12);
            break;
        case 0x12F:
            flags = (flags & ~0xFF0u) | ((token->value & 0xFF) << 4);
            break;
        case 0xFFFF:
            if (token->type == 4)
            {
                if (token->value == 0x4B)
                {
                    flags |= 0x400000;
                }
                else if (token->value == 0x4D)
                {
                    flags |= 0x800000;
                }
            }

            break;
        default:
            break;
        }

        reader.Next();
    }
}

void SetKeyCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->kind)
        {
        case 0x7:
            keyAndFlags = (keyAndFlags & ~0xFFu) | ((token->value - 1) & 0xFF);
            break;
        case 0x5A:
            keyAndFlags = (keyAndFlags & ~0xFF00u) | (((token->value - 1) & 0xFF) << 8);
            break;
        case 0x5B:
            keyAndFlags = (keyAndFlags & ~0xFF0000u) | (((token->value - 1) & 0xFF) << 16);
            break;
        case 0x58:
            keyAndFlags |= 0x1000000;
            break;
        case 0xFFFF:
            if (token->type == 4)
            {
                switch (token->value)
                {
                case 0xC2:
                    keyAndFlags |= 0x2000000;
                    break;
                case 0xD2:
                    keyAndFlags |= 0x4000000;
                    break;
                case 0xFC:
                    keyAndFlags &= ~0x8000000u;
                    break;
                case 0xC4:
                    keyAndFlags |= 0x10000000;
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

void DoParticleCommand::ParseTokens(const ScriptTokenList* tokens)
{
    // systemAndFlags' bit 24: from the exit point's frame
    constexpr u32 FromExitPoint = 0x1000000;
    u32 exitPoint = ExitPointMask;
    u32 system = SystemMask;
    u32 value = 1;
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    f32 x = 0.0f;
    f32 y = 0.0f;
    f32 z = 0.0f;
    flags2 = (flags2 & ~Turned & ~HasPosition & ~(AxesMask << AxesShift)) | NoneByte << DesignatorShift |
             NoneByte << SurfaceShift | NoneByte << KeyShift;
    systemAndFlags = (systemAndFlags | FromKeyPending) & ~FromExitPoint;
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
        case 0x7:
            flags2 = (flags2 & ~(NoneByte << KeyShift)) | ((token->value - 1) & NoneByte) << KeyShift;
            break;
        case 0xA:
            value = token->value;
            break;
        case 0x12:
            exitPoint = token->value;
            break;
        case 0x15:
            system = token->value;
            break;
        case 0x104:
            flags2 = (flags2 & ~(NoneByte << DesignatorShift)) |
                     (TokenDesignator(token, (flags2 >> DesignatorShift) & NoneByte) & NoneByte) << DesignatorShift;
            flags2 |= HasPosition;
            systemAndFlags &= ~FromKeyPending;
            break;
        case 0xFFFF:
            if (token->type == 4)
            {
                switch (token->value)
                {
                case 0x2A:
                    flags2 &= ~(AxesMask << AxesShift);
                    break;
                case 0x2B:
                    flags2 = (flags2 & ~(AxesMask << AxesShift)) | 1 << AxesShift;
                    break;
                case 0x26:
                    flags2 = (flags2 & ~(AxesMask << AxesShift)) | 2 << AxesShift;
                    break;
                case 0x27:
                    flags2 = (flags2 & ~(AxesMask << AxesShift)) | 3 << AxesShift;
                    break;
                case 0x28:
                    flags2 = (flags2 & ~(AxesMask << AxesShift)) | 4 << AxesShift;
                    break;
                case 0x29:
                    flags2 = (flags2 & ~(AxesMask << AxesShift)) | 5 << AxesShift;
                    break;
                case 0xE:
                    systemAndFlags &= ~FromKeyPending;
                    break;
                case 0x17:
                    flags2 |= Turned;
                    break;
                case 0xD5:
                    flags2 &= ~(NoneByte << SurfaceShift);
                    break;
                case 0xE7:
                    flags2 = (flags2 & ~(NoneByte << SurfaceShift)) | 1 << SurfaceShift;
                    break;
                case 0xE8:
                    flags2 = (flags2 & ~(NoneByte << SurfaceShift)) | 2 << SurfaceShift;
                    break;
                case 0xF7:
                    flags2 = (flags2 & ~(NoneByte << SurfaceShift)) | 3 << SurfaceShift;
                    break;
                case 0xD7:
                    flags2 = (flags2 & ~(NoneByte << SurfaceShift)) | 4 << SurfaceShift;
                    break;
                case 0xD9:
                    flags2 = (flags2 & ~(NoneByte << SurfaceShift)) | 5 << SurfaceShift;
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

    systemAndFlags = (systemAndFlags & ~SystemMask) | (system & SystemMask);
    systemAndFlags = (systemAndFlags & ~(ValueMask << ValueShift)) | (value & ValueMask) << ValueShift;
    if (x != 0.0f || y != 0.0f || z != 0.0f)
    {
        posX = __builtin_bit_cast(u32, x);
        posY = __builtin_bit_cast(u32, y);
        posZ = __builtin_bit_cast(u32, z);
        posW = __builtin_bit_cast(u32, 1.0f);
        flags2 |= HasPosition;
    }

    if (exitPoint < ExitPointMask)
    {
        systemAndFlags = ((systemAndFlags | FromExitPoint) & ~(ExitPointMask << ExitPointShift)) |
                         (exitPoint & ExitPointMask) << ExitPointShift;
    }
    else
    {
        systemAndFlags = (systemAndFlags & ~FromExitPoint) | ExitPointMask << ExitPointShift;
    }
}

void DUMMY_197Command::ParseTokens(const ScriptTokenList* tokens)
{
    // A position that isn't 0 goes into offsetX to value9 (floats, value9 the w) and sets objectId's bit 27
    f32 x = 0.0f;
    f32 y = 0.0f;
    f32 z = 0.0f;
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->kind)
        {
        case 0x82:
            x = token->Float();
            break;
        case 0x83:
            y = token->Float();
            break;
        case 0x84:
            z = token->Float();
            break;
        case 0xC:
            value2 = __builtin_bit_cast(s32, token->Float());
            break;
        case 0x12:
            objectId = ((objectId | 0x100000) & ~0x7E00000u) | ((token->value & 0x3F) << 21);
            break;
        case 0x89:
            objectId = (objectId & ~0xFFFFu) | (token->value & 0xFFFF);
            break;
        default:
            break;
        }

        reader.Next();
    }

    if (x != 0.0f || y != 0.0f || z != 0.0f)
    {
        offsetX = __builtin_bit_cast(s32, x);
        offsetY = __builtin_bit_cast(s32, y);
        offsetZ = __builtin_bit_cast(s32, z);
        value9 = __builtin_bit_cast(u32, 1.0f);
        objectId |= 0x8000000;
    }
}

void AddTrailCommand::ParseTokens(const ScriptTokenList* tokens)
{
    // flags2 is more bits here, not a tagged value
    f32 x = 0.0f;
    f32 y = 0.0f;
    f32 z = 0.0f;
    u32 exitPoint = 0x3F;
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
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
        case 0xB:
            flags2.raw |= 0x4;
            scale = token->Float();
            break;
        case 0x12:
            exitPoint = token->value;
            break;
        case 0x15:
            ids1 = (ids1 & ~0xFFFFu) | (token->value & 0xFFFF);
            break;
        case 0x16:
        {
            // A list of halfwords from ids1's high half on, its count in flags2's bits 14-17 (past the eleventh, into
            // value11 and on)
            u32 count = static_cast<u32>(flags2.raw >> 14) & 0xF;
            flags2.raw = (flags2.raw & ~0x3C000) | static_cast<s32>(((count + 1) & 0xF) << 14);
            reinterpret_cast<u16*>(&ids1)[1 + count] = static_cast<u16>(token->value);
            break;
        }
        case 0x92:
            switch (token->value)
            {
            case 0x67:
                flags &= ~0x3800000u;
                break;
            case 0x68:
                flags = (flags & ~0x3800000u) | 0x800000;
                break;
            case 0x6A:
                flags = (flags & ~0x3800000u) | 0x1000000;
                break;
            case 0x69:
                flags = (flags & ~0x3800000u) | 0x1800000;
                break;
            default:
                break;
            }

            // Retail has no break here: the value goes on into ids5's high half
            [[fallthrough]];
        case 0x17:
            ids5 = (ids5 & 0xFFFF) | (token->value << 16);
            break;
        case 0x18:
        {
            f32 number = token->Float();
            flags = (flags & ~0xFu) | 0x4;
            value11 = number * __builtin_fabsf(number);
            break;
        }
        case 0x19:
        {
            f32 number = token->Float();
            flags = (flags & ~0xFu) | 0x3;
            value11 = number * __builtin_fabsf(number);
            break;
        }
        case 0x1A:
            flags = (flags & ~0xFu) | 0x1;
            value12 = token->Float();
            break;
        case 0x1B:
            flags |= 0x4000000;
            value25 = token->Float();
            break;
        case 0x1C:
        {
            f32 number = token->Float();
            flags = (flags & ~0xFu) | 0x5;
            value11 = number * __builtin_fabsf(number);
            break;
        }
        case 0x6D:
            ids6 = (ids6 & ~0xFFFFu) | (token->value & 0xFFFF);
            break;
        case 0x97:
            flags2.raw |= 0x10;
            value21 = token->Float();
            break;
        case 0x98:
            flags2.raw |= 0x8;
            value22 = token->Float();
            break;
        case 0x9E:
            flags |= 0x8000000;
            value11 = token->Float();
            break;
        case 0xE0:
            flags2.raw |= 0x1;
            value15 = __builtin_bit_cast(s32, token->Float());
            break;
        case 0xE1:
            flags2.raw |= 0x1;
            value16 = token->Float();
            break;
        case 0xE2:
            flags2.raw |= 0x1;
            value17 = __builtin_bit_cast(s32, token->Float());
            break;
        case 0xE4:
            flags2.raw |= 0x1;
            value19 = token->Float();
            break;
        case 0xE5:
            flags = (flags & ~0xFu) | 0x7;
            flags2.raw |= 0x2;
            value23 = token->Float();
            break;
        case 0xFE:
            flags = (flags & ~0xFu) | 0x8;
            value24 = token->Float();
            break;
        case 0x116:
            flags = (flags & ~0xFu) | 0x1;
            value13 = token->Float();
            break;
        case 0x128:
            flags2.raw = (flags2.raw & ~0x3C00000) | static_cast<s32>((token->value & 0xF) << 22);
            break;
        case 0xFFFF:
            if (token->type == 4)
            {
                switch (token->value)
                {
                case 0x3:
                    flags = (flags & ~0x3C00u) | 0x400;
                    break;
                case 0x4:
                    flags = (flags & ~0x3C00u) | 0x800;
                    break;
                case 0x5:
                    flags = (flags & ~0x3C00u) | 0xC00;
                    break;
                case 0xF:
                    flags &= ~0xFu;
                    break;
                case 0x17:
                    flags |= 0x40000;
                    break;
                case 0x2A:
                    flags &= ~0x780000u;
                    break;
                case 0x2B:
                    flags = (flags & ~0x780000u) | 0x80000;
                    break;
                case 0x26:
                    flags = (flags & ~0x780000u) | 0x100000;
                    break;
                case 0x27:
                    flags = (flags & ~0x780000u) | 0x180000;
                    break;
                case 0x28:
                    flags = (flags & ~0x780000u) | 0x200000;
                    break;
                case 0x29:
                    flags = (flags & ~0x780000u) | 0x280000;
                    break;
                case 0xD5:
                    flags2.raw &= ~0x3FC0;
                    break;
                case 0xE7:
                    flags2.raw = (flags2.raw & ~0x3FC0) | 0x40;
                    break;
                case 0xE8:
                    flags2.raw = (flags2.raw & ~0x3FC0) | 0x80;
                    break;
                case 0xF7:
                    flags2.raw = (flags2.raw & ~0x3FC0) | 0xC0;
                    break;
                case 0xD7:
                    flags2.raw = (flags2.raw & ~0x3FC0) | 0x100;
                    break;
                case 0xD9:
                    flags2.raw = (flags2.raw & ~0x3FC0) | 0x140;
                    break;
                case 0xE9:
                    flags2.raw = (flags2.raw & ~0x1C0000) | 0x40000;
                    break;
                case 0xEA:
                    flags2.raw = (flags2.raw & ~0x1C0000) | 0x80000;
                    break;
                case 0x10C:
                    flags2.raw |= 0x200000;
                    break;
                case 0x112:
                    flags2.raw |= 0x4000000;
                    break;
                default:
                    // Any other keyword clears flags' bits 10-13, as 3, 4 and 5 set them
                    flags &= ~0x3C00u;
                    break;
                }
            }

            break;
        default:
            break;
        }

        reader.Next();
    }

    if ((flags & 0xF) == 1)
    {
        f32 width = value13;
        value13 = width + width;
        value12 = value12 + width;
    }

    if (x != 0.0f || y != 0.0f || z != 0.0f)
    {
        Vector4 position = {x, y, z, 1.0f};
        SetTrailPosition(&flags, &position);
    }

    if (exitPoint != 0x3F)
    {
        reinterpret_cast<u8*>(&ids6)[2] = static_cast<u8>(exitPoint);
    }
}
