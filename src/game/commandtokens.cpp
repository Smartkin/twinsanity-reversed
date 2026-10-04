#include "game/commands.h"

#include "game/math.h"
#include "game/progress.h"
#include "game/scripttokens.h"

// The commands' development tools parsers (their vtables' slot 2): each token's kind names an argument, its value goes into the
// command's field. The retail game never calls them

void SetChiChiGrassCommand::ParseTokens(const ScriptTokenList* tokens)
{
    // The setting's given bit (low half) and its value (high half): token 0 sets it, 1 clears it
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        if (token->kind == 0x210)
        {
            if (token->value == 0)
            {
                value1 = (value1 | 0x1) | 0x10000;
            }
            else if (token->value == 1)
            {
                value1 = (value1 | 0x1) & ~0x10000u;
            }
        }

        reader.Next();
    }
}

void DUMMY_SetRayTestsCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->kind)
        {
        case 0x211:
            value1 = static_cast<s32>(token->value);
            break;
        case 0x212:
            value2 = static_cast<s32>(token->value);
            break;
        case 0x213:
            value3 = static_cast<s32>(token->value);
            break;
        case 0x214:
            value4 = static_cast<s32>(token->value);
            break;
        case 0x57:
            value5 = static_cast<s32>(token->value);
            break;
        default:
            break;
        }

        reader.Next();
    }
}

void BecomeStickyCommand::ParseTokens(const ScriptTokenList* tokens)
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
            if (token->type == 4)
            {
                switch (token->value)
                {
                case 0x20D:
                    value1 |= 0x2000;
                    break;
                case 0x20E:
                    value1 |= 0x8000;
                    break;
                case 0x20F:
                    value1 |= 0x1000;
                    break;
                case 0x210:
                    value1 |= 0x10000;
                    break;
                default:
                    break;
                }
            }

            break;
        case 0x23A:
            value2 = token->Float();
            break;
        case 0x10:
            objectId = (objectId & ~0xFFFFu) | (token->value & 0xFFFF);
            break;
        case 0x6D:
            message = static_cast<s32>(token->value);
            break;
        default:
            break;
        }

        reader.Next();
    }
}

void BecomeNormalCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        if (token->kind == 0xFFFF && token->type == 4)
        {
            if (token->value == 0)
            {
                value1 |= 0x1;
            }
            else if (token->value == 1)
            {
                value1 &= ~0x1u;
            }
        }

        reader.Next();
    }
}

void SetObjectFlags587Command::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->kind)
        {
        case 0x23C:
            flags.raw = (flags.raw & ~0x3) | (TokenSetting(token) & 0x3);
            break;
        case 0x240:
            flags.raw = (flags.raw & ~0xC0) | ((TokenSetting(token) & 0x3) << 6);
            break;
        case 0x23D:
            flags.raw = (flags.raw & ~0xC) | ((TokenSetting(token) & 0x3) << 2);
            break;
        case 0x23E:
            flags.raw = (flags.raw & ~0x30) | ((TokenSetting(token) & 0x3) << 4);
            break;
        case 0x23F:
            flags.raw = (flags.raw & ~0xC00) | ((TokenSetting(token) & 0x3) << 10);
            break;
        default:
            break;
        }

        reader.Next();
    }
}

void SetNode5FlagsCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->kind)
        {
        case 0x220:
            flags.raw = (flags.raw & ~0x3) | (TokenIsZero(token) ? 1 : 2);
            break;
        case 0x221:
            flags.raw = (flags.raw & ~0xC) | ((TokenIsZero(token) ? 1 : 2) << 2);
            break;
        case 0x222:
            flags.raw = (flags.raw & ~0x30) | ((TokenIsZero(token) ? 1 : 2) << 4);
            break;
        case 0x223:
            flags.raw = (flags.raw & ~0xC0) | ((TokenIsZero(token) ? 1 : 2) << 6);
            break;
        default:
            break;
        }

        reader.Next();
    }
}

void CreateCrateContentsCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->kind)
        {
        case 0x201:
            value1 = (value1 & ~0xFFFFu) | (token->value & 0xFFFF);
            break;
        case 0x202:
            value1 = (value1 & 0xFFFF) | (token->value << 16);
            break;
        case 0x218:
            value2 = (value2 & ~0xFu) | (token->value & 0xF);
            break;
        case 0x219:
            value2 = (value2 & ~0xF0u) | ((token->value & 0xF) << 4);
            break;
        default:
            break;
        }

        reader.Next();
    }
}

void CounterPositionOp579Command::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->kind)
        {
        case 0x1C:
            counter &= ~0x20000u;
            break;
        case 0x6E:
            counter = (counter & ~0xFFFFu) | (token->value & 0xFFFF);
            break;
        case 0xB2:
            counter = (counter & ~0xFFFFu) | (token->value & 0xFFFF);
            counter |= 0x10000;
            break;
        default:
            break;
        }

        reader.Next();
    }
}

void CharacterSoundProxyCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        if (token->kind == 0xFFFF && token->type == 4)
        {
            if (token->value == 0x88)
            {
                value1 |= 0x100;
            }
            else
            {
                value1 = (value1 & ~0xFFu) | (TokenCharacter(token->value) & 0xFF);
            }
        }

        reader.Next();
    }
}

void DamageOriginatorCommand::ParseTokens(const ScriptTokenList* tokens)
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
            switch (token->value)
            {
            case 0x21B:
                contactWord |= 0x2;
                break;
            case 0x21C:
                contactWord |= 0x4;
                break;
            case 0x21D:
                contactWord |= 0x8;
                break;
            case 0x21E:
                contactWord |= 0x10;
                break;
            case 0x21F:
                contactWord |= 0x20;
                break;
            case 0x220:
                contactWord |= 0x40;
                break;
            case 0x221:
                contactWord |= 0x80;
                break;
            case 0x222:
                contactWord |= 0x100;
                break;
            case 0x224:
                contactWord |= 0x800;
                break;
            case 0x225:
                contactWord |= 0x1000;
                break;
            case 0x226:
                contactWord |= 0x2000;
                break;
            case 0x227:
                contactWord |= 0x4000;
                break;
            case 0x228:
                contactWord |= 0x8000;
                break;
            case 0x229:
                contactWord |= 0x10000;
                break;
            case 0x22A:
                contactWord |= 0x20000;
                break;
            case 0x236:
                contactWord |= 0x1000000;
                break;
            case 0x22B:
                contactWord |= 0x40000;
                break;
            case 0x22C:
                contactWord |= 0x80000;
                break;
            case 0x22E:
                contactWord |= 0x400000;
                break;
            case 0x22D:
                contactWord |= 0x200000;
                break;
            case 0x22F:
                contactWord |= 0x800000;
                break;
            default:
                break;
            }

            break;
        case 0x204:
            ParseTaggedValueRecord(token, &hitPoints);
            break;
        default:
            break;
        }

        reader.Next();
    }
}

void HitInstancesInBoxesCommand::ParseTokens(const ScriptTokenList* tokens)
{
    // What commands.h has as the radius is the word the execution puts in its contact message (DamageOriginator's contactWord)
    auto& contactWord = *reinterpret_cast<u32*>(&radius);
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->kind)
        {
        case 0x6D:
            flags |= 0x4;
            event = static_cast<u16>(token->value);
            break;
        case 0xFFFF:
            switch (token->value)
            {
            case 0x206:
                flags |= 0x1;
                break;
            case 0x245:
                flags |= 0x2;
                break;
            case 0x21B:
                contactWord |= 0x2;
                break;
            case 0x21C:
                contactWord |= 0x4;
                break;
            case 0x21D:
                contactWord |= 0x8;
                break;
            case 0x21E:
                contactWord |= 0x10;
                break;
            case 0x21F:
                contactWord |= 0x20;
                break;
            case 0x220:
                contactWord |= 0x40;
                break;
            case 0x221:
                contactWord |= 0x80;
                break;
            case 0x222:
                contactWord |= 0x100;
                break;
            case 0x224:
                contactWord |= 0x800;
                break;
            case 0x225:
                contactWord |= 0x1000;
                break;
            case 0x226:
                contactWord |= 0x2000;
                break;
            case 0x227:
                contactWord |= 0x4000;
                break;
            case 0x228:
                contactWord |= 0x8000;
                break;
            case 0x229:
                contactWord |= 0x10000;
                break;
            case 0x22A:
                contactWord |= 0x20000;
                break;
            case 0x236:
                contactWord |= 0x1000000;
                break;
            case 0x22B:
                contactWord |= 0x40000;
                break;
            case 0x22C:
                contactWord |= 0x80000;
                break;
            case 0x22E:
                contactWord |= 0x400000;
                break;
            case 0x22D:
                contactWord |= 0x200000;
                break;
            case 0x22F:
                contactWord |= 0x800000;
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

void CutsceneCameraOp583Command::ParseTokens(const ScriptTokenList* tokens)
{
    constexpr f32 Pi = 0x1.921fb6p+1f;
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
            y = static_cast<s32>(token->value);
            break;
        case 0x2:
            value2.raw = static_cast<s32>(token->value);
            break;
        case 0x59:
            value3.raw = static_cast<s32>(token->value);
            break;
        case 0x65:
            value4 = token->Float();
            break;
        case 0xD9:
            value5 = token->Float();
            break;
        case 0xDA:
            value6 = token->Float();
            break;
        case 0x26:
            value7.raw = static_cast<s32>(token->value);
            break;
        case 0x27:
            value8 = static_cast<s32>(token->value);
            break;
        case 0x20:
            value9.raw = static_cast<s32>(token->value);
            break;
        case 0x21:
            value10 = static_cast<s32>(token->value);
            break;
        case 0xD8:
            value11.raw = static_cast<s32>(token->value);
            break;
        default:
            break;
        }

        reader.Next();
    }

    // Most of what the tokens gave is then replaced by random and fixed values (plain floats, the tagged ones too)
    value7.raw = __builtin_bit_cast(s32, RandomSignedTimes(Pi));
    value9.raw = __builtin_bit_cast(s32, RandomSignedTimes(0.25f) + 1.0f);
    value11.raw = __builtin_bit_cast(s32, RandomSignedTimes(0.5f) + 0.5f);
    value13.raw = __builtin_bit_cast(s32, RandomSignedTimes(1.0f) + 2.0f);
    value5 = 150.0f;
    value6 = 200.0f;
    value3.raw = __builtin_bit_cast(s32, __builtin_bit_cast(f32, value3.raw) * 0.5f);
    value15 = -2.0f;
    value4 = 2.0f;
    value2.raw = __builtin_bit_cast(s32, RandomSignedTimes(1.0f) + 5.5f);
}

void CutsceneCameraMoveCommand::ParseTokens(const ScriptTokenList* tokens)
{
    constexpr f32 DegreesToAngle = 0x1.6C16C2p+7f;
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->kind)
        {
        case 0x76:
            offset1 = token->Float();
            break;
        case 0x10D:
            offset2 = token->Float();
            break;
        case 0x114:
            offset5 = token->Float();
            break;
        case 0x115:
            offset6 = token->Float();
            break;
        case 0x241:
            offset3 = token->Float();
            break;
        case 0x242:
            offset4 = token->Float();
            break;
        case 0x245:
            unused8 = token->value;
            break;
        case 0x246:
            unused9 = token->value;
            break;
        case 0x247:
            flagsAndAngle.raw |= 0x200000;
            value12 = static_cast<s32>(token->Float() * DegreesToAngle);
            break;
        case 0x13B:
            value10 = static_cast<s32>(token->value);
            break;
        case 0x13C:
            value11 = static_cast<s32>(token->value);
            break;
        case 0xFFFF:
            if (token->type == 4)
            {
                switch (token->value)
                {
                case 0x253:
                    flagsAndAngle.raw &= ~0x7;
                    break;
                case 0x254:
                    flagsAndAngle.raw = (flagsAndAngle.raw & ~0x7) | 0x1;
                    break;
                case 0x255:
                    flagsAndAngle.raw = (flagsAndAngle.raw & ~0x7) | 0x2;
                    break;
                case 0x256:
                    flagsAndAngle.raw = (flagsAndAngle.raw & ~0x7) | 0x3;
                    break;
                case 0x26A:
                    flagsAndAngle.raw = (flagsAndAngle.raw & ~0x7) | 0x4;
                    break;
                case 0x257:
                    flagsAndAngle.raw &= ~0x38;
                    break;
                case 0x258:
                    flagsAndAngle.raw = (flagsAndAngle.raw & ~0x38) | 0x8;
                    break;
                case 0x259:
                    flagsAndAngle.raw = (flagsAndAngle.raw & ~0x38) | 0x10;
                    break;
                case 0x25A:
                    flagsAndAngle.raw = (flagsAndAngle.raw & ~0x38) | 0x18;
                    break;
                case 0x25B:
                    flagsAndAngle.raw = (flagsAndAngle.raw & ~0x38) | 0x20;
                    break;
                case 0x25C:
                    flagsAndAngle.raw = (flagsAndAngle.raw & ~0x38) | 0x28;
                    break;
                case 0x25D:
                    flagsAndAngle.raw = (flagsAndAngle.raw & ~0x38) | 0x30;
                    break;
                case 0x25E:
                    flagsAndAngle.raw |= 0x38;
                    break;
                case 0x25F:
                    flagsAndAngle.raw &= ~0x1C0;
                    break;
                case 0x260:
                    flagsAndAngle.raw = (flagsAndAngle.raw & ~0x1C0) | 0x40;
                    break;
                case 0x261:
                    flagsAndAngle.raw = (flagsAndAngle.raw & ~0x1C0) | 0x80;
                    break;
                case 0x267:
                    flagsAndAngle.raw = (flagsAndAngle.raw & ~0x1C0) | 0xC0;
                    break;
                case 0x264:
                    flagsAndAngle.raw &= ~0xE00;
                    break;
                case 0x265:
                    flagsAndAngle.raw = (flagsAndAngle.raw & ~0xE00) | 0x200;
                    break;
                case 0x266:
                    flagsAndAngle.raw = (flagsAndAngle.raw & ~0xE00) | 0x400;
                    break;
                case 0x268:
                    flagsAndAngle.raw &= ~0x7000;
                    break;
                case 0x269:
                    flagsAndAngle.raw = (flagsAndAngle.raw & ~0x7000) | 0x1000;
                    break;
                case 0xF5:
                    flagsAndAngle.raw |= 0x8000;
                    break;
                case 0x26B:
                    flagsAndAngle.raw |= 0x10000;
                    break;
                case 0x26C:
                    flagsAndAngle.raw |= 0x20000;
                    break;
                case 0xA8:
                    flagsAndAngle.raw &= ~0x1C0000;
                    break;
                case 0xF6:
                    flagsAndAngle.raw = (flagsAndAngle.raw & ~0x1C0000) | 0x40000;
                    break;
                case 0x26D:
                    flagsAndAngle.raw |= 0x400000;
                    break;
                case 0x26E:
                    flagsAndAngle.raw |= 0x800000;
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

void ToggleCutsceneCameraCommand::ParseTokens(const ScriptTokenList* tokens)
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
            blendTime = token->Float();
            break;
        case 0xFFFF:
            if (token->type == 4)
            {
                switch (token->value)
                {
                case 0x262:
                    modeFlags &= ~0x7u;
                    break;
                case 0x263:
                    modeFlags = (modeFlags & ~0x7u) | 0x1;
                    break;
                case 0xFA:
                    modeFlags = (modeFlags & ~0x7u) | 0x2;
                    break;
                case 0xEB:
                    modeFlags |= 0x10;
                    break;
                case 0xA8:
                    modeFlags &= ~0xE0u;
                    break;
                case 0xF6:
                    modeFlags = (modeFlags & ~0xE0u) | 0x40;
                    break;
                case 0xDB:
                    modeFlags |= 0x8;
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

void CutsceneCameraTargetsCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->kind)
        {
        case 0x107:
            targets = (targets & ~0xFFu) | (TokenDesignator(token, targets & 0xFF) & 0xFF);
            break;
        case 0x108:
            targets = (targets & ~0xFF00u) | ((TokenDesignator(token, (targets >> 8) & 0xFF) & 0xFF) << 8);
            break;
        case 0x110:
            targets = (targets & ~0xFF0000u) | ((TokenDesignator(token, (targets >> 16) & 0xFF) & 0xFF) << 16);
            break;
        case 0x111:
            targets = (targets & ~0xFF000000u) | ((TokenDesignator(token, targets >> 24) & 0xFF) << 24);
            break;
        case 0x139:
            keys = (keys & ~0xFFu) | (token->value & 0xFF);
            break;
        case 0x13A:
            keys = (keys & ~0xFF00u) | ((token->value & 0xFF) << 8);
            break;
        case 0xFFFF:
            if (token->type == 4)
            {
                if (token->value == 0xDC)
                {
                    flags |= 0x1;
                }
                else if (token->value == 0xDD)
                {
                    flags &= ~0x2u;
                }
            }

            break;
        default:
            break;
        }

        reader.Next();
    }
}

void SetCameraNodeValueCommand::ParseTokens(const ScriptTokenList* tokens)
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
            if (token->type == 4)
            {
                if (token->value == 0x115)
                {
                    mode.raw &= ~0x7;
                }
                else if (token->value == 0x116)
                {
                    mode.raw = (mode.raw & ~0x7) | 0x1;
                }
            }

            break;
        case 0xD6:
            value = token->Float();
            break;
        default:
            break;
        }

        reader.Next();
    }
}

void SetCameraNodeValuesCommand::ParseTokens(const ScriptTokenList* tokens)
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
            if (token->type == 4)
            {
                switch (token->value)
                {
                case 0x117:
                    mode.raw &= ~0x7;
                    break;
                case 0x118:
                    mode.raw = (mode.raw & ~0x7) | 0x1;
                    break;
                case 0x119:
                    mode.raw = (mode.raw & ~0x7) | 0x2;
                    break;
                case 0x11A:
                    mode.raw = (mode.raw & ~0x7) | 0x3;
                    break;
                default:
                    break;
                }
            }

            break;
        case 0xD9:
            value1 = token->Float();
            break;
        case 0xDA:
            value2 = token->Float();
            break;
        default:
            break;
        }

        reader.Next();
    }
}

void SetPlayerModeCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->kind)
        {
        case 0x203:
            value1 = TokenPlayerMode(token->value);
            break;
        case 0x249:
            value2 = TokenCharacter(token->value);
            break;
        case 0x248:
            value3 = TokenCharacter(token->value);
            break;
        default:
            break;
        }

        reader.Next();
    }
}

void FadeoutScreenCommand::ParseTokens(const ScriptTokenList* tokens)
{
    // A colour that isn't 0 sets the flags' bit 3
    constexpr f32 Epsilon = 0x1.a36e2ep-15f;
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->kind)
        {
        case 0xFFFF:
            if (token->type == 4)
            {
                if (token->value == 0x14)
                {
                    flags &= ~0x7u;
                }
                else if (token->value == 0x15)
                {
                    flags = (flags & ~0x7u) | 0x1;
                }
            }

            break;
        case 0xC:
            duration = token->Float();
            break;
        case 0x59:
            unused3 = static_cast<s32>(token->value);
            break;
        case 0xD:
            red = token->Float();
            flags |= (__builtin_fabsf(red) <= Epsilon ? 0u : 1u) << 3;
            break;
        case 0xE:
            green = token->Float();
            flags |= (__builtin_fabsf(green) <= Epsilon ? 0u : 1u) << 3;
            break;
        case 0xF:
            blue = token->Float();
            flags |= (__builtin_fabsf(blue) <= Epsilon ? 0u : 1u) << 3;
            break;
        default:
            break;
        }

        reader.Next();
    }
}

void DisplayBottomTextCommand::ParseTokens(const ScriptTokenList* tokens)
{
    x = 0.0f;
    y = 0.0f;
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->kind)
        {
        case 0xFFFF:
            if (token->type == 4)
            {
                switch (token->value)
                {
                case 0xEC:
                    value4 = 1.0f;
                    break;
                case 0xED:
                    value5 = 1.0f;
                    break;
                case 0xEE:
                    value6 = 1.0f;
                    break;
                case 0xEF:
                    value5 = 1.0f;
                    value6 = 1.0f;
                    break;
                case 0xF0:
                    value4 = 1.0f;
                    value6 = 1.0f;
                    break;
                case 0xF1:
                    value4 = 1.0f;
                    value5 = 1.0f;
                    break;
                case 0xF2:
                    value4 = 0.0f;
                    value5 = 0.0f;
                    value6 = 0.0f;
                    break;
                case 0xF3:
                    value4 = 1.0f;
                    value5 = 1.0f;
                    value6 = 1.0f;
                    break;
                case 0xF4:
                    x = 0.5f;
                    y = Rounded(0.92);
                    break;
                default:
                    break;
                }
            }

            break;
        case 0xE9:
            value1 = static_cast<s32>(token->value);
            break;
        case 0x0:
            x = token->Float();
            break;
        case 0x1:
            y = token->Float();
            break;
        case 0xD:
            value4 = token->Float();
            break;
        case 0xE:
            value5 = token->Float();
            break;
        case 0xF:
            value6 = token->Float();
            break;
        case 0xC:
            value7 = token->Float();
            break;
        default:
            break;
        }

        reader.Next();
    }
}

void DisplayBottomTextInstanceCommand::ParseTokens(const ScriptTokenList* tokens)
{
    x = 0.0f;
    y = 0.0f;
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->kind)
        {
        case 0xFFFF:
            if (token->type == 4)
            {
                switch (token->value)
                {
                case 0xEC:
                    value3 = 1.0f;
                    break;
                case 0xED:
                    value4 = 1.0f;
                    break;
                case 0xEE:
                    value5 = 1.0f;
                    break;
                case 0xEF:
                    value4 = 1.0f;
                    value5 = 1.0f;
                    break;
                case 0xF0:
                    value3 = 1.0f;
                    value5 = 1.0f;
                    break;
                case 0xF1:
                    value3 = 1.0f;
                    value4 = 1.0f;
                    break;
                case 0xF2:
                    value3 = 0.0f;
                    value4 = 0.0f;
                    value5 = 0.0f;
                    break;
                case 0xF3:
                    value3 = 1.0f;
                    value4 = 1.0f;
                    value5 = 1.0f;
                    break;
                case 0xF4:
                    x = 0.5f;
                    y = Rounded(0.92);
                    break;
                default:
                    break;
                }
            }

            break;
        case 0x0:
            x = token->Float();
            break;
        case 0x1:
            y = token->Float();
            break;
        case 0xD:
            value3 = token->Float();
            break;
        case 0xE:
            value4 = token->Float();
            break;
        case 0xF:
            value5 = token->Float();
            break;
        case 0xC:
            value6 = token->Float();
            break;
        default:
            break;
        }

        reader.Next();
    }
}

void PlayMovieCommand::ParseTokens(const ScriptTokenList* tokens)
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
            if (token->type != 1)
            {
                break;
            }

            [[fallthrough]];
        case 0xC3:
            value.raw &= ~TaggedValue::TypeMask;
            ParseTaggedValueRecord(token, &value);
            break;
        case 0xC:
            value2 = token->Float();
            break;
        default:
            break;
        }

        reader.Next();
    }
}

void RaycastFocusPositionCommand::ParseTokens(const ScriptTokenList* tokens)
{
    // The vector's components are kept until the end, its w made 1
    f32 vectorX = 0.0f;
    f32 vectorY = 0.0f;
    f32 vectorZ = 0.0f;
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->kind)
        {
        case 0xFFFF:
            if (token->type == 4)
            {
                switch (token->value)
                {
                case 0x3:
                    mode.raw = 1;
                    break;
                case 0x4:
                    mode.raw = 2;
                    break;
                case 0x5:
                    mode.raw = 3;
                    break;
                default:
                    mode.raw = 0;
                    break;
                }
            }

            break;
        case 0x3C:
            distance = -token->Float();
            break;
        case 0xCD:
            size = static_cast<s32>(token->value);
            break;
        case 0x0:
            vectorX = token->Float();
            break;
        case 0x1:
            vectorY = token->Float();
            break;
        case 0x2:
            vectorZ = token->Float();
            break;
        case 0x104:
            target = (target & ~0xFFu) | (TokenDesignator(token, target & 0xFF) & 0xFF);
            break;
        default:
            break;
        }

        reader.Next();
    }

    x = vectorX;
    y = vectorY;
    z = vectorZ;
    value5 = 1.0f;
}

void SetFocusPositionToNearestPointCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->kind)
        {
        case 0xC6:
            unused = token->Float();
            target |= 0x100;
            break;
        case 0x6:
            target = (target & ~0xFFu) | (token->value & 0xFF);
            break;
        default:
            break;
        }

        reader.Next();
    }
}

void RequestFocusCommand::ParseTokens(const ScriptTokenList* tokens)
{
    constexpr f32 Epsilon = 0x1.a36e2ep-15f;
    // The first keyword that sets bits of flags11 clears the ones it had
    bool clearFlags11 = true;
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        targetFlags = (targetFlags & ~0xFF00u) | ((TokenDesignator(token, (targetFlags >> 8) & 0xFF) & 0xFF) << 8);
        targetFlags = (targetFlags & ~0xFu) | (TokenSpace(token, targetFlags & 0xF) & 0xF);
        TokenVectorComponent(token, &x);
        switch (token->kind)
        {
        case 0xFFFF:
            if (token->type == 4)
            {
                switch (token->value)
                {
                case 0x47:
                    targetFlags = (targetFlags & ~0x7800000u) | 0x800000;
                    break;
                case 0x48:
                    targetFlags = (targetFlags & ~0x7800000u) | 0x1000000;
                    break;
                case 0x49:
                    targetFlags = (targetFlags & ~0x7800000u) | 0x1800000;
                    break;
                case 0x4A:
                    targetFlags = (targetFlags & ~0x7800000u) | 0x2000000;
                    break;
                case 0x4C:
                    targetFlags = (targetFlags & ~0x18000000u) | 0x8000000;
                    break;
                case 0x77:
                    targetFlags |= 0x40000000;
                    break;
                case 0x4B:
                    targetFlags = (targetFlags & ~0x18000000u) | 0x10000000;
                    break;
                case 0x4D:
                    targetFlags |= 0x20000000;
                    break;
                case 0xDE:
                    flags10 |= 0x40;
                    break;
                case 0x39:
                    targetFlags = (targetFlags & ~0x80000u) | 0x400000;
                    break;
                case 0x204:
                    flags11 &= ~0x2000;
                    break;
                case 0x205:
                    flags11 &= ~0x8000;
                    break;
                case 0x207:
                    flags11 &= ~0x10000;
                    break;
                case 0x208:
                    flags11 &= ~0x401800;
                    break;
                case 0x209:
                    flags11 &= ~0x4000;
                    break;
                case 0x20A:
                    flags11 &= ~0x80000;
                    break;
                case 0x20B:
                    flags11 &= ~0x20000;
                    break;
                case 0x231:
                    flags11 &= ~0x100000;
                    break;
                case 0x20C:
                    flags11 &= ~0x40000;
                    break;
                case 0x20D:
                    if (clearFlags11)
                    {
                        flags11 = 0;
                        clearFlags11 = false;
                    }

                    flags11 |= 0x2000;
                    break;
                case 0x20E:
                    if (clearFlags11)
                    {
                        flags11 = 0;
                        clearFlags11 = false;
                    }

                    flags11 |= 0x8000;
                    break;
                case 0x211:
                    if (clearFlags11)
                    {
                        flags11 = 0;
                        clearFlags11 = false;
                    }

                    flags11 |= 0x401800;
                    break;
                case 0x230:
                    if (clearFlags11)
                    {
                        flags11 = 0;
                        clearFlags11 = false;
                    }

                    flags11 |= 0x100000;
                    break;
                case 0x212:
                    if (clearFlags11)
                    {
                        flags11 = 0;
                        clearFlags11 = false;
                    }

                    flags11 |= 0x4000;
                    break;
                case 0x213:
                    if (clearFlags11)
                    {
                        flags11 = 0;
                        clearFlags11 = false;
                    }

                    flags11 |= 0x80000;
                    break;
                case 0x214:
                    if (clearFlags11)
                    {
                        flags11 = 0;
                        clearFlags11 = false;
                    }

                    flags11 |= 0x20000;
                    break;
                case 0x215:
                    if (clearFlags11)
                    {
                        flags11 = 0;
                        clearFlags11 = false;
                    }

                    flags11 |= 0x40000;
                    break;
                case 0xB0:
                    flags10 |= 0x4;
                    break;
                case 0x252:
                    flags10 |= 0x20;
                    break;
                default:
                    break;
                }
            }

            break;
        case 0x7B:
        {
            // A list of halfwords from ids8 on, its count in flags10's bits 3-4 (past the second, into targetFlags)
            u32 count = (flags10 >> 3) & 0x3;
            flags10 = (flags10 & ~0x18u) | (((count + 1) & 0x3) << 3);
            reinterpret_cast<u16*>(&ids8)[count] = static_cast<u16>(token->value);
            targetFlags = (targetFlags & ~0x7800000u) | 0x2800000;
            break;
        }
        case 0x6:
            targetFlags = (targetFlags & ~0xF0u) | ((token->value & 0xF) << 4);
            break;
        case 0x10:
        {
            // A list of halfwords from ids6 on, its count in targetFlags' bits 16-18 (past the sixth, into targetFlags)
            u32 count = (targetFlags >> 16) & 0x7;
            targetFlags = (targetFlags & ~0x70000u) | (((count + 1) & 0x7) << 16);
            reinterpret_cast<u16*>(&ids6)[count] = static_cast<u16>(token->value);
            break;
        }
        case 0x65:
            radius = token->Float();
            break;
        default:
            break;
        }

        reader.Next();
    }

    if (!(__builtin_fabsf(x) <= Epsilon && __builtin_fabsf(y) <= Epsilon && __builtin_fabsf(z) <= Epsilon))
    {
        targetFlags |= 0x100000;
    }
}

void SetFocusPropertiesCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->kind)
        {
        case 0x207:
            value1.raw = (value1.raw & ~0x3) | (TokenSetting(token) & 0x3);
            break;
        case 0x208:
            value1.raw = (value1.raw & ~0xC) | ((TokenSetting(token) & 0x3) << 2);
            break;
        case 0x20B:
            value1.raw = (value1.raw & ~0x30) | ((TokenSetting(token) & 0x3) << 4);
            break;
        case 0x20A:
            value1.raw = (value1.raw & ~0xC0) | ((TokenSetting(token) & 0x3) << 6);
            break;
        case 0x216:
            value1.raw = (value1.raw & ~0x300) | ((TokenSetting(token) & 0x3) << 8);
            break;
        default:
            break;
        }

        reader.Next();
    }
}

void CreateHeadTrackingCommand::ParseTokens(const ScriptTokenList* tokens)
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
            switch (token->value)
            {
            case 0xE1:
                animations |= 0x1000000;
                break;
            case 0xE2:
                animations |= 0x2000000;
                break;
            case 0xE3:
                animations |= 0x4000000;
                break;
            default:
                break;
            }

            break;
        case 0x65:
            range = token->Float() * token->Float();
            break;
        case 0xB9:
            value0 = static_cast<s32>(token->value);
            break;
        case 0xB8:
            value2 = token->Float();
            break;
        case 0xB7:
            SetHeadTrackingAngle3(&value0, token->Float());
            break;
        case 0xB5:
            SetHeadTrackingAngle1(&value0, token->Float());
            break;
        case 0xB6:
            SetHeadTrackingAngle2(&value0, token->Float());
            break;
        case 0xBA:
            value9 = static_cast<s32>(token->value);
            break;
        case 0xBB:
            value10 = static_cast<s32>(token->value);
            break;
        case 0xBC:
            value11 = static_cast<s32>(token->value);
            break;
        case 0xBD:
            value12 = static_cast<s32>(token->value);
            break;
        case 0xBE:
            value13 = static_cast<s32>(token->value);
            break;
        case 0xBF:
            value14 = static_cast<s32>(token->value);
            break;
        case 0xB3:
            animations = (animations & ~0xFFu) | (token->value & 0xFF);
            break;
        case 0xB4:
            animations = (animations & ~0xFF00u) | ((token->value & 0xFF) << 8);
            break;
        case 0x12:
            animations = (animations & ~0xFF0000u) | ((token->value & 0xFF) << 16);
            break;
        case 0x3F:
            speed = token->Float();
            break;
        case 0x9B:
            flags = token->value * token->value;
            break;
        default:
            break;
        }

        reader.Next();
    }
}

void SetKeyNearestPlayerCommand::ParseTokens(const ScriptTokenList* tokens)
{
    constexpr f32 Epsilon = Rounded(1e-05);
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->kind)
        {
        case 0x3C:
            if (Epsilon < token->Float())
            {
                value1 = token->Float() * token->Float();
            }

            break;
        case 0x11B:
            value2 = token->Float();
            break;
        case 0x5A:
            value3 = (value3 & ~0xFF00u) | (((token->value - 1) & 0xFF) << 8);
            break;
        case 0x5B:
            value3 = (value3 & ~0xFF0000u) | (((token->value - 1) & 0xFF) << 16);
            break;
        case 0xFFFF:
            if (token->value == 0x252)
            {
                value3 = (value3 & ~0xFFu) | 0x1;
            }

            break;
        default:
            break;
        }

        reader.Next();
    }
}

void GetShortRouteCommand::ParseTokens(const ScriptTokenList* tokens)
{
    constexpr f32 Epsilon = 0x1.a36e2ep-15f;
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        targetFlags = (targetFlags & ~0xF0000000u) | ((TokenSpace(token, targetFlags >> 28) & 0xF) << 28);
        targetFlags = (targetFlags & ~0xFF00u) | ((TokenDesignator(token, (targetFlags >> 8) & 0xFF) & 0xFF) << 8);
        TokenVectorComponent(token, &x);
        switch (token->kind)
        {
        case 0xFFFF:
            switch (token->value)
            {
            case 0x21A:
                flags7 &= ~0x2u;
                targetFlags &= ~0x4000000u;
                break;
            case 0x239:
                flags7 &= ~0x10u;
                targetFlags &= ~0x4000000u;
                break;
            case 0x23A:
                targetFlags &= ~(0x20000u | 0x4000000u);
                break;
            case 0x218:
                targetFlags &= ~(0x40000u | 0x4000000u);
                if ((targetFlags & 0x80000) == 0)
                {
                    targetFlags &= ~0x20000u;
                }

                break;
            case 0x219:
                targetFlags &= ~(0x80000u | 0x4000000u);
                if ((targetFlags & 0x40000) == 0)
                {
                    targetFlags &= ~0x20000u;
                }

                break;
            case 0x238:
                flags7 &= ~0x8u;
                targetFlags &= ~0x4000000u;
                break;
            case 0x237:
                flags7 &= ~0x4u;
                targetFlags &= ~0x4000000u;
                break;
            case 0x217:
                targetFlags &= ~(0x100000u | 0x4000000u);
                break;
            case 0x23B:
                flags7 &= ~(0x4u | 0x8u | 0x10u);
                targetFlags &= ~(0x20000u | 0x100000u | 0x4000000u);
                break;
            case 0x23D:
                flags7 &= ~(0x2u | 0x4u | 0x8u);
                targetFlags &= ~(0x100000u | 0x4000000u);
                break;
            case 0x23C:
                flags7 &= ~(0x2u | 0x4u | 0x8u | 0x10u);
                targetFlags &= ~(0x20000u | 0x4000000u);
                break;
            case 0x23E:
                unknown14 |= 0x56;
                break;
            case 0x23F:
                keyAndObject |= 0x40000;
                break;
            case 0x240:
                keyAndObject |= 0x20000;
                break;
            case 0x241:
                keyAndObject |= 0x100000;
                break;
            case 0x242:
                keyAndObject |= 0x400000;
                break;
            case 0x24D:
                unknown15 |= 0x56;
                break;
            case 0x24E:
                unknown14 |= 0x40000;
                break;
            case 0x24F:
                unknown14 |= 0x20000;
                break;
            case 0x250:
                unknown14 |= 0x100000;
                break;
            case 0x251:
                unknown14 |= 0x400000;
                break;
            default:
                break;
            }

            break;
        case 0x6:
            targetFlags = (targetFlags & ~0xFFu) | (token->value & 0xFF);
            break;
        case 0xA6:
            ParseTaggedValueRecord(token, &value8);
            targetFlags = (targetFlags | 0x1000000) & ~0x4000000u;
            break;
        case 0xA7:
            ParseTaggedValueRecord(token, &value9);
            targetFlags = (targetFlags | 0x800000) & ~0x4000000u;
            break;
        case 0x63:
            ParseTaggedValueRecord(token, &value10);
            targetFlags = (targetFlags | 0x400000) & ~0x4000000u;
            break;
        case 0xAD:
            ParseTaggedValueRecord(token, &value11);
            targetFlags = (targetFlags | 0x8000000) & ~0x4000000u;
            break;
        case 0xC6:
            ParseTaggedValueRecord(token, &value12);
            flags7 |= 0x1;
            break;
        case 0xF7:
            value16 = token->Float();
            break;
        case 0xF8:
            value17 = token->Float();
            break;
        case 0x62:
            keyAndObject = (keyAndObject & ~0xFFu) | (token->value & 0xFF);
            break;
        default:
            break;
        }

        reader.Next();
    }

    if (!(__builtin_fabsf(x) <= Epsilon && __builtin_fabsf(y) <= Epsilon && __builtin_fabsf(z) <= Epsilon))
    {
        targetFlags |= 0x2000000;
    }

    if ((targetFlags & 0xC0000) == 0)
    {
        targetFlags &= ~0x20000u;
    }
}

void SetNearestPointFlagsCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->kind)
        {
        case 0xAF:
            flags.raw = (flags.raw & ~0x3) | (TokenIsZero(token) ? 1 : 2);
            break;
        case 0xB0:
            flags.raw = (flags.raw & ~0xC) | ((TokenIsZero(token) ? 1 : 2) << 2);
            break;
        case 0xB1:
            flags.raw = (flags.raw & ~0x30) | ((TokenIsZero(token) ? 1 : 2) << 4);
            break;
        case 0x65:
            range = static_cast<s32>(token->value);
            flags.raw |= 0x40;
            break;
        case 0xAE:
            value = static_cast<s32>(token->value);
            flags.raw |= 0x40;
            break;
        default:
            break;
        }

        reader.Next();
    }
}

void CreateNodeControllerCommand::ParseTokens(const ScriptTokenList* tokens)
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
            case 0x29A:
                controller &= ~0xFFu;
                break;
            case 0x29B:
                controller = (controller & ~0xFFu) | 0x1;
                break;
            case 0x29C:
                controller = (controller & ~0xFFu) | 0x2;
                break;
            case 0x29D:
                controller = (controller & ~0xFFu) | 0x3;
                break;
            case 0xFD:
                controller |= 0x100;
                break;
            default:
                break;
            }
        }

        reader.Next();
    }
}

void AddPerceptionCommand::ParseTokens(const ScriptTokenList* tokens)
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
            switch (token->value)
            {
            case 0xAD:
                type.raw &= ~0x7;
                break;
            case 0xAF:
                type.raw = (type.raw & ~0x7) | 0x2;
                break;
            case 0xAE:
                type.raw = (type.raw & ~0x7) | 0x1;
                break;
            case 0xB3:
                type.raw = (type.raw & ~0x7) | 0x3;
                break;
            case 0x211:
                type.raw |= 0x8;
                break;
            default:
                break;
            }

            break;
        case 0xD3:
            value1 = token->Float();
            break;
        case 0x65:
            // The range and its square, as floats
            range = static_cast<s32>(token->value);
            range2 = __builtin_bit_cast(u32, token->Float() * token->Float());
            break;
        case 0xD8:
            value11 = token->Float();
            break;
        case 0xD4:
            value10 = static_cast<s32>(token->value);
            break;
        case 0x10:
        {
            // A list of halfwords from objectId on, its count the bytes' first (past the eighth, into the bytes)
            u32 count = bytes & 0xFF;
            bytes = (bytes & ~0xFFu) | ((count + 1) & 0xFF);
            reinterpret_cast<u16*>(&objectId)[count] = static_cast<u16>(token->value);
            break;
        }
        case 0xD9:
            value12 = static_cast<s32>(token->value);
            break;
        case 0xDA:
            value13 = token->Float();
            break;
        case 0xDB:
            value14 = static_cast<s32>(token->value);
            break;
        case 0x243:
            value15 = token->Float();
            break;
        default:
            break;
        }

        reader.Next();
    }
}
