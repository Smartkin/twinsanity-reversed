#include "game/commands.h"

#include "game/math.h"
#include "game/progress.h"
#include "game/properties.h"
#include "game/scripttokens.h"

// More of the commands' development tools parsers (their vtables' slot 2, game/commandtokens.cpp has the others): each token's
// kind names an argument, its value goes into the command's field. The retail game never calls them

namespace
{
// The vtable's parser of one token, which the commands sharing a class have each of their own
constexpr u32 ParseTokenSlot = 7;

// A setting's given bit (in the word's low half) and its value (the high half): a token of 0 sets it, 1 clears it, any other
// changes neither
u32 GivenSetting(u32 word, const ScriptToken* token, u32 bit)
{
    if (token->value == 0)
    {
        return word | bit | bit << 16;
    }

    if (token->value == 1)
    {
        return (word | bit) & ~(bit << 16);
    }

    return word;
}

// A tagged value's type set, the rest of it kept
void SetTaggedType(TaggedValue* value, TaggedValue::Type type)
{
    value->raw = (value->raw & ~TaggedValue::TypeMask) | static_cast<s32>(type << TaggedValue::TypeShift);
}
}

void FinalBossInitWeaponsCommand::ParseTokens(const ScriptTokenList* tokens)
{
    // The mode tells the commands sharing the class apart, each with arguments of its own
    ScriptTokenReader reader;
    switch (mode)
    {
    case 0:
        ScriptTokenReader::Construct(&reader, tokens);
        reader.First();
        while (!reader.AtEnd())
        {
            const ScriptToken* token = reader.Current();
            switch (token->kind)
            {
            case 0x133:
                slots = (slots & ~0xFFu) | (token->value & 0xFF);
                break;
            case 0x134:
                slots = (slots & ~0xFF00u) | ((token->value & 0xFF) << 8);
                break;
            case 0x135:
                slots = (slots & ~0xFF0000u) | ((token->value & 0xFF) << 16);
                break;
            default:
                break;
            }

            reader.Next();
        }

        break;
    case 1:
    case 2:
        ScriptTokenReader::Construct(&reader, tokens);
        reader.First();
        while (!reader.AtEnd())
        {
            const ScriptToken* token = reader.Current();
            if (token->kind == 0xFFFF)
            {
                switch (token->value)
                {
                case 0x123:
                    weapons |= 0x1;
                    break;
                case 0x124:
                    weapons |= 0x2;
                    break;
                case 0x125:
                    weapons |= 0x4;
                    break;
                default:
                    break;
                }
            }

            reader.Next();
        }

        break;
    case 3:
        ScriptTokenReader::Construct(&reader, tokens);
        reader.First();
        while (!reader.AtEnd())
        {
            const ScriptToken* token = reader.Current();
            if (token->kind == 0xC7)
            {
                target = (target & ~0xFFu) | (TokenDesignator(token, target & 0xFF) & 0xFF);
            }

            reader.Next();
        }

        break;
    case 4:
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
                case 0x123:
                    weapons |= 0x1;
                    break;
                case 0x124:
                    weapons |= 0x2;
                    break;
                case 0x125:
                    weapons |= 0x4;
                    break;
                default:
                    break;
                }

                break;
            case 0x59:
                value1 = __builtin_bit_cast(s32, token->Float());
                break;
            case 0xD6:
                value2 = __builtin_bit_cast(s32, token->Float());
                break;
            default:
                break;
            }

            reader.Next();
        }

        break;
    default:
        break;
    }
}

void SetSplineControllerValuesCommand::ParseTokens(const ScriptTokenList* tokens)
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
            SetTaggedType(&x, TaggedValue::TypeFloat);
            ParseTaggedValueRecord(token, &x);
            break;
        case 0x1:
            SetTaggedType(&y, TaggedValue::TypeFloat);
            ParseTaggedValueRecord(token, &y);
            break;
        case 0x2:
            SetTaggedType(&z, TaggedValue::TypeFloat);
            ParseTaggedValueRecord(token, &z);
            break;
        case 0x18:
            SetTaggedType(&value4, TaggedValue::TypeFloat);
            ParseTaggedValueRecord(token, &value4);
            break;
        case 0x78:
            SetTaggedType(&value5, TaggedValue::TypeFloat);
            ParseTaggedValueRecord(token, &value5);
            break;
        case 0x13D:
            SetTaggedType(&value6, TaggedValue::TypeFloat);
            ParseTaggedValueRecord(token, &value6);
            break;
        case 0x13E:
            SetTaggedType(&value7, TaggedValue::TypeFloat);
            ParseTaggedValueRecord(token, &value7);
            break;
        default:
            break;
        }

        reader.Next();
    }
}

void SetSkateControllerIdsCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->kind)
        {
        case 0x15:
        {
            // A list of halfwords from ids1 on, its count in the counts' bits 0-3 (past the twelfth, into the sounds)
            u32 count = counts & 0xF;
            counts = (counts & ~0xFu) | ((count + 1) & 0xF);
            reinterpret_cast<u16*>(&ids1)[count] = static_cast<u16>(token->value);
            break;
        }
        case 0x16:
        {
            // A list of halfwords from sounds1 on (the unused words are its rest), its count in the counts' bits 4-8
            u32 count = (counts >> 4) & 0x1F;
            counts = (counts & ~0x1F0u) | (((count + 1) & 0x1F) << 4);
            reinterpret_cast<u16*>(&sounds1)[count] = static_cast<u16>(token->value);
            break;
        }
        case 0xFFFF:
            if (token->value == 0x128)
            {
                counts |= 0x200;
            }

            break;
        default:
            break;
        }

        reader.Next();
    }
}

void SetPlayerInputCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->kind)
        {
        case 0x229:
            inputFlags = GivenSetting(inputFlags, token, 0x1);
            break;
        case 0x22A:
            inputFlags = GivenSetting(inputFlags, token, 0x2);
            break;
        case 0x22B:
            inputFlags = GivenSetting(inputFlags, token, 0x4);
            break;
        case 0x22C:
            inputFlags = GivenSetting(inputFlags, token, 0x8);
            break;
        case 0x22D:
            inputFlags = GivenSetting(inputFlags, token, 0x10);
            break;
        case 0x22E:
            inputFlags = GivenSetting(inputFlags, token, 0x20);
            break;
        case 0x20D:
            inputFlags = GivenSetting(inputFlags, token, 0x40);
            break;
        case 0x230:
            inputFlags = GivenSetting(inputFlags, token, 0x80);
            break;
        case 0x237:
            inputFlags = GivenSetting(inputFlags, token, 0x100);
            break;
        case 0x81:
            unknown2 = (unknown2 & ~0x2u) | (static_cast<u32>(!TokenIsZero(token)) << 1);
            break;
        case 0xFFFF:
            if (token->value == 0xB4)
            {
                unknown2 &= ~0x1u;
            }
            else if (token->value == 0xB5)
            {
                unknown2 |= 0x1;
            }

            break;
        default:
            break;
        }

        reader.Next();
    }
}

void CharacterOp578Command::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->kind)
        {
        case 0x6E:
            value = (value & ~0xFFFFu) | (token->value & 0xFFFF);
            break;
        case 0xB2:
            value = (value & ~0xFFFFu) | (token->value & 0xFFFF);
            value |= 0x10000;
            break;
        case 0xFFFF:
            if (token->value == 0xA3)
            {
                value |= 0x20000;
            }
            else if (token->value == 0xA4)
            {
                value &= ~0x20000u;
            }

            break;
        default:
            break;
        }

        reader.Next();
    }
}

void SetPlayerFlag57Command::ParseTokens(const ScriptTokenList* tokens)
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
                value |= 0x1;
            }
            else if (token->value == 1)
            {
                value &= ~0x1u;
            }
        }

        reader.Next();
    }
}

void TriggerCharacterEvent12Command::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        if (token->kind == 0xFFFF && token->type == 4)
        {
            switch (token->value)
            {
            case 0x3E:
                characters |= 0x1;
                break;
            case 0x232:
                characters |= 0x2;
                break;
            case 0x233:
                characters |= 0x4;
                break;
            case 0x29E:
                characters |= 0x8;
                break;
            default:
                break;
            }
        }

        reader.Next();
    }
}

void SwitchCharacterCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->kind)
        {
        case 0x72:
            value1 = (value1 & ~0xFu) | ((token->value - 1) & 0xF) | 0x10;
            break;
        case 0xFFFF:
            if (token->type == 4)
            {
                // Bits 5-12 a designator: the focus object, AgentRef1
                if (token->value == 0x1C)
                {
                    value1 = (value1 & ~0x1FE0u) | 0xFB << 5;
                }
                else if (token->value == 0x74)
                {
                    value1 = (value1 & ~0x1FE0u) | 0xF8 << 5;
                }
                else
                {
                    value2 = static_cast<s32>(TokenCharacter(token->value));
                }
            }

            break;
        default:
            break;
        }

        reader.Next();
    }
}

void SetAgentCommand::ParseTokens(const ScriptTokenList* tokens)
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
            agentFlags = GivenSetting(agentFlags, token, 0x1);
            break;
        case 0x208:
            agentFlags = GivenSetting(agentFlags, token, 0x2);
            break;
        case 0x20B:
            agentFlags = GivenSetting(agentFlags, token, 0x4);
            break;
        case 0x20A:
            agentFlags = GivenSetting(agentFlags, token, 0x8);
            break;
        case 0x209:
            agentFlags = GivenSetting(agentFlags, token, 0x10);
            break;
        case 0x20F:
            agentFlags = GivenSetting(agentFlags, token, 0x20);
            break;
        case 0x216:
            agentFlags = GivenSetting(agentFlags, token, 0x40);
            break;
        case 0x237:
            agentFlags = GivenSetting(agentFlags, token, 0x80);
            break;
        case 0x238:
            agentFlags = GivenSetting(agentFlags, token, 0x100);
            break;
        case 0x244:
            agentFlags = GivenSetting(agentFlags, token, 0x200);
            break;
        default:
            break;
        }

        reader.Next();
    }
}

void ApplyVelocityCommand::ParseTokens(const ScriptTokenList* tokens)
{
    // Shared with ApplyVelocityToSelf (526): each command's own parser of a token takes them one by one
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        CallVirtual<void>(this, vtable, ParseTokenSlot, reader.Current());
        reader.Next();
    }
}

void NowGoBackCollidableCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        if (token->kind == 0x59)
        {
            ParseTaggedValueRecord(token, &value1);
        }

        reader.Next();
    }
}

void SetNode120FlagCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        if (token->kind == 0xFFFF && token->type == 4 && token->value == 0xFD)
        {
            value |= 0x1;
        }

        reader.Next();
    }
}

void ReduceHitPointsCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        if (token->kind == 0x204)
        {
            hitPoints = static_cast<s32>(token->value);
        }

        reader.Next();
    }
}

void SetHitPointsCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        if (token->kind == 0x204)
        {
            hitPoints = token->value;
        }

        reader.Next();
    }
}

void SetNodeValue174Command::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        if (token->kind == 0x136)
        {
            value = token->Float();
        }

        reader.Next();
    }
}

void CreateDamageCommand::ParseTokens(const ScriptTokenList* tokens)
{
    // Shared with 546: each command's own parser of a token takes them one by one
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        CallVirtual<void>(this, vtable, ParseTokenSlot, reader.Current());
        reader.Next();
    }
}

void SetCameraCommand::ParseTokens(const ScriptTokenList* tokens)
{
    // value1's bits 0 and 1 say which of the others were given
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->kind)
        {
        case 0x76:
            SetTaggedType(&value2, TaggedValue::TypeAngle);
            ParseTaggedValueRecord(token, &value2);
            value1.raw |= 0x1;
            break;
        case 0x77:
            value1.raw |= 0x2;
            value3 = __builtin_bit_cast(s32, token->Float());
            break;
        default:
            break;
        }

        reader.Next();
    }
}

void SetGlobalProgressionCommand::ParseTokens(const ScriptTokenList* tokens)
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
            SetTaggedType(&value, TaggedValue::TypeInt);
            ParseTaggedValueRecord(token, &value);
            break;
        case 0xFFFF:
            value1 = static_cast<u32>(TokenArea(token->value));
            break;
        default:
            break;
        }

        reader.Next();
    }
}

void SetGlobalProgression2Command::ParseTokens(const ScriptTokenList* tokens)
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
            SetTaggedType(&value, TaggedValue::TypeInt);
            ParseTaggedValueRecord(token, &value);
            break;
        case 0xFFFF:
            value1 = TokenArea(token->value);
            break;
        default:
            break;
        }

        reader.Next();
    }
}

void CutsceneStartCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        if (token->kind == 0xC)
        {
            value1 = token->Float();
        }

        reader.Next();
    }
}

void CutsceneEndCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        if (token->kind == 0xC)
        {
            value1 = token->Float();
        }

        reader.Next();
    }
}

void EnableBossModeCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->kind)
        {
        case 0x11:
            animSlots = static_cast<s32>(token->value);
            break;
        case 0x204:
            ParseTaggedValueRecord(token, &hitPoints);
            break;
        case 0xE6:
            value3 = token->Float();
            break;
        default:
            break;
        }

        reader.Next();
    }
}

void DamageBossCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        if (token->kind == 0xFFFF)
        {
            value1 = token->value;
        }

        reader.Next();
    }
}

void StartWhackawormCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->kind)
        {
        case 0x11:
            animSlots = static_cast<s32>(token->value);
            break;
        case 0xC:
            ParseTaggedValueRecord(token, &value2);
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

void ProgressWhackawormCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        if (token->kind == 0xFFFF)
        {
            value1 = token->value;
        }

        reader.Next();
    }
}

void ShowBottomTextCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        if (token->kind == 0xC)
        {
            value1 = token->Float();
        }

        reader.Next();
    }
}

void HideBottomTextCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        if (token->kind == 0xC)
        {
            value1 = token->Float();
        }

        reader.Next();
    }
}

void RequestOgiSlotCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        if (token->kind == 0x11)
        {
            slot = (slot & ~0xFFu) | (token->value & 0xFF);
        }

        reader.Next();
    }
}

void SetFocusToGameActorCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        if (token->kind == 0x239)
        {
            actorIndex = (actorIndex & ~0xFFu) | (TokenCharacter(token->value) & 0xFF);
        }

        reader.Next();
    }
}

void DUMMY_586Command::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        if (token->kind == 0xE9)
        {
            value1 = (value1 & ~0xFFFFu) | (token->value & 0xFFFF);
        }

        reader.Next();
    }
}

void SetMaskControllerIdsCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        if (token->kind == 0x15)
        {
            // A list of halfwords from ids1 on (the unused words are its rest), its count in the count's bits 0-3. A retail bug: the
            // count goes up to 15, and past the twelfth the halfwords overwrite the count and then what follows the command
            u32 index = count & 0xF;
            count = (count & ~0xFu) | ((index + 1) & 0xF);
            reinterpret_cast<u16*>(&ids1)[index] = static_cast<u16>(token->value);
        }

        reader.Next();
    }
}

void AddGemCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        if (token->kind == 0xFFFF)
        {
            value1 = TokenGem(token->value);
        }

        reader.Next();
    }
}

void CA_PickUpWumpaCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        if (token->kind == 0x200)
        {
            value1 = static_cast<s32>(token->value);
        }

        reader.Next();
    }
}

void SetPlayerRespawnPositionCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->kind)
        {
        case 0x205:
            value1 = (value1 & ~0xFFu) | (token->value == 0 ? 1u : 0u);
            break;
        case 0x23B:
            value1 = (value1 & ~0xFF00u) | (token->value == 0 ? 1u : 0u) << 8;
            break;
        default:
            break;
        }

        reader.Next();
    }
}

void LinkToFocusCharacterCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        if (token->kind == 0x8A)
        {
            if (token->value == 0x56)
            {
                value1 &= ~0x1u;
            }
            else if (token->value == 0x57)
            {
                value1 |= 0x1;
            }
        }

        reader.Next();
    }
}

void AddLivesCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        if (token->kind == 0x22F)
        {
            value1 = static_cast<s32>(token->value);
        }

        reader.Next();
    }
}

void DismissCharacterCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        if (token->kind == 0x239)
        {
            value1 = static_cast<s32>(TokenCharacter(token->value));
        }

        reader.Next();
    }
}

void PlaceCharacterInChunkCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        if (token->kind == 0x239)
        {
            character = static_cast<s32>(TokenCharacter(token->value));
        }

        reader.Next();
    }
}

void SetPlayerVehicleValueCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        if (token->kind == 0xFFFF && token->type == 0)
        {
            value = token->Float();
        }

        reader.Next();
    }
}

void SetCrateCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        if (token->kind == 0x20E)
        {
            value2 = token->value;
        }

        reader.Next();
    }
}

void ApplyVelocityToHeldBodyCommand::ParseTokens(const ScriptTokenList* tokens)
{
    // The velocity starts as the zero point (w 1)
    *reinterpret_cast<Vector4*>(&x) = g_DefaultBox.min;
    w = 1.0f;
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->kind)
        {
        case 0x49:
            x = token->Float();
            break;
        case 0x4A:
            y = token->Float();
            break;
        case 0x4B:
            z = token->Float();
            break;
        default:
            break;
        }

        reader.Next();
    }
}

void CA_SetPickupCommand::ParseTokens(const ScriptTokenList* tokens)
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
            hitPoints = (hitPoints & ~0x1u) | static_cast<u32>(TokenIsZero2(token));
            break;
        case 0x221:
            hitPoints = (hitPoints & ~0x2u) | static_cast<u32>(TokenIsZero2(token)) << 1;
            break;
        case 0x222:
            hitPoints = (hitPoints & ~0x4u) | static_cast<u32>(TokenIsZero2(token)) << 2;
            break;
        case 0x223:
            hitPoints = (hitPoints & ~0x8u) | static_cast<u32>(TokenIsZero2(token)) << 3;
            break;
        case 0x225:
            hitPoints = (hitPoints & ~0x10u) | static_cast<u32>(TokenIsZero2(token)) << 4;
            break;
        case 0x226:
            hitPoints = (hitPoints & ~0x10u) | static_cast<u32>(TokenIsZero2(token)) << 4;
            hitPoints = (hitPoints & ~0x20u) | static_cast<u32>(TokenIsZero2(token)) << 5;
            break;
        case 0x224:
            hitPoints = (hitPoints & ~0x20u) | static_cast<u32>(TokenIsZero2(token)) << 5;
            break;
        case 0x21C:
            hitPoints = (hitPoints & ~0x40u) | static_cast<u32>(TokenIsZero2(token)) << 6;
            break;
        case 0x21D:
            hitPoints = (hitPoints & ~0x80u) | static_cast<u32>(TokenIsZero2(token)) << 7;
            break;
        case 0x204:
            hitPoints = (hitPoints & ~0x300u) | (token->value & 0x3) << 8;
            break;
        case 0x21E:
            value1 = token->Float();
            hitPoints |= 0x400;
            break;
        case 0x21F:
            value2 = token->Float();
            hitPoints |= 0x400;
            break;
        case 0x227:
            value3 = token->Float();
            break;
        default:
            break;
        }

        reader.Next();
    }
}

void CA_SetProjectileCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->kind)
        {
        case 0x227:
            speed = token->Float();
            break;
        case 0x90:
            value4 = token->Float();
            break;
        case 0x78:
            hitPoints |= 0x10;
            value3 = __builtin_bit_cast(s32, token->Float());
            break;
        case 0x7A:
            hitPoints |= 0x40;
            value5 = __builtin_bit_cast(s32, token->Float());
            break;
        case 0x6A:
            ParseTaggedValueRecord(token, &radius);
            hitPoints |= 0x20;
            break;
        case 0x204:
            hitPoints = (hitPoints & ~0xFu) | (token->value & 0xF);
            break;
        case 0xFFFF:
            if (token->value == 0x82)
            {
                hitPoints |= 0x80;
            }

            break;
        default:
            break;
        }

        reader.Next();
    }
}

void ShootCommand::ParseTokens(const ScriptTokenList* tokens)
{
    // An offset that isn't 0 sets its bit
    constexpr f32 Epsilon = 0x1.a36e2ep-15f;
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        TokenVectorComponent(token, &x);
        switch (token->kind)
        {
        case 0x89:
            objectAndMessage = (objectAndMessage & ~0xFFFFu) | (token->value & 0xFFFF);
            break;
        case 0x6D:
            objectAndMessage = (objectAndMessage & 0xFFFF) | token->value << 16;
            break;
        case 0x12:
            shot = (shot & ~ExitPointMask) | (token->value & ExitPointMask);
            break;
        case 0x39:
            shot |= HasSpeed;
            speed = token->Float();
            break;
        case 0xFFFF:
            if (token->type == 4)
            {
                switch (token->value)
                {
                case 0x2A:
                    shot &= ~0x700u;
                    break;
                case 0x2B:
                    shot = (shot & ~0x700u) | 0x100;
                    break;
                case 0x26:
                    shot = (shot & ~0x700u) | 0x200;
                    break;
                case 0x27:
                    shot = (shot & ~0x700u) | 0x300;
                    break;
                case 0x28:
                    shot = (shot & ~0x700u) | 0x400;
                    break;
                case 0x29:
                    shot = (shot & ~0x700u) | 0x500;
                    break;
                case 0x17:
                    shot |= 0x1000;
                    break;
                case 0x79:
                    shot |= AtTarget;
                    break;
                case 0x299:
                    shot |= Bit15;
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
        shot |= Offset;
    }
}

void DUMMY_568Command::ParseTokens(const ScriptTokenList* tokens)
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
        case 0xA2:
            value1 = __builtin_bit_cast(s32, token->Float());
            break;
        case 0xA3:
            value2 = __builtin_bit_cast(s32, token->Float());
            break;
        case 0x1C:
            value3 = __builtin_bit_cast(s32, token->Float());
            break;
        case 0x7A:
            value4 = __builtin_bit_cast(s32, token->Float());
            break;
        case 0x231:
            shorts1 = (shorts1 & ~0xFFFFu) | (token->value & 0xFFFF);
            break;
        case 0x232:
            shorts1 = (shorts1 & 0xFFFF) | token->value << 16;
            break;
        case 0x233:
            shorts2 = (shorts2 & ~0xFFFFu) | (token->value & 0xFFFF);
            break;
        case 0x235:
            shorts2 = (shorts2 & 0xFFFF) | token->value << 16;
            break;
        case 0x234:
            shorts3 = (shorts3 & ~0xFFFFu) | (token->value & 0xFFFF);
            break;
        default:
            break;
        }

        reader.Next();
    }
}

void SetVehicleHumiliskateCommand::ParseTokens(const ScriptTokenList* tokens)
{
    // Floats of 25 and 35 unless the tokens give others
    SetTaggedType(&value2, TaggedValue::TypeFloat);
    value2.SetFloat(25.0f);
    SetTaggedType(&value3, TaggedValue::TypeFloat);
    value3.SetFloat(35.0f);
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->kind)
        {
        case 0x10C:
            ParseTaggedValueRecord(token, &value2);
            break;
        case 0x10B:
            ParseTaggedValueRecord(token, &value3);
            break;
        case 0x249:
            value1 = (value1 & ~0xFFu) | (TokenCharacter(token->value) & 0xFF);
            break;
        case 0x248:
            value1 = (value1 & ~0xFF00u) | (TokenCharacter(token->value) & 0xFF) << 8;
            break;
        default:
            break;
        }

        reader.Next();
    }
}

void SetVehicleHoverboardCommand::ParseTokens(const ScriptTokenList* tokens)
{
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
        case 0xFFFF:
            if (token->value == 0x297)
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

void SetMotionCommand::ParseTokens(const ScriptTokenList* tokens)
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
        case 0xA6:
            range = __builtin_bit_cast(s32, token->Float());
            break;
        case 0xAE:
            value2 = __builtin_bit_cast(s32, token->Float());
            break;
        case 0xA7:
            value3 = __builtin_bit_cast(s32, token->Float());
            break;
        case 0xA8:
            value4 = __builtin_bit_cast(s32, token->Float());
            break;
        case 0xFFFF:
            if (token->type == 4 && token->value == 0x83)
            {
                flags = (flags & ~0x1Fu) | 0x1;
            }

            break;
        default:
            break;
        }

        reader.Next();
    }
}

void SetLinkedObjectNearestPlayerCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->kind)
        {
        case 0x12F:
            range = (range & ~0xFFu) | (token->value & 0xFF);
            break;
        case 0x12E:
            range = (range & ~0xFF00u) | (token->value & 0xFF) << 8;
            break;
        default:
            break;
        }

        reader.Next();
    }
}

void DUMMY_NowGoForwardCollidableCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ParseTaggedValueTokens(tokens, &angleValue);
}

void AttachAllLinkedAgentsCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        if (token->kind == 0x12)
        {
            value1 = ((value1 | 0x1) & ~0x7Eu) | (token->value & 0x3F) << 1;
        }

        reader.Next();
    }
}

void SetVehicleRollerbrawlCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->kind)
        {
        case 0x249:
            value1 = (value1 & ~0xFFu) | (TokenCharacter(token->value) & 0xFF);
            break;
        case 0x248:
            value1 = (value1 & ~0xFF00u) | (TokenCharacter(token->value) & 0xFF) << 8;
            break;
        default:
            break;
        }

        reader.Next();
    }
}

void ExitVehicleModeCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        if (token->kind == 0x249)
        {
            value1 = static_cast<s32>(TokenCharacter(token->value));
        }

        reader.Next();
    }
}

void AddAmmoCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        if (token->kind == 0x200)
        {
            value1 = static_cast<s32>(token->value);
        }

        reader.Next();
    }
}

void SetBehaviourPriorityCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->kind)
        {
        case 0x64:
            if (token->type != 4)
            {
                priorityValue = (priorityValue & ~0xFFu) | (token->value & 0xFF);
            }
            else if (token->value == 0x2F)
            {
                priorityValue |= 0xFF;
            }

            break;
        case 0xFFFF:
            if (token->type == 4 && token->value == 0x2F)
            {
                priorityValue |= 0xFF;
            }

            break;
        default:
            break;
        }

        reader.Next();
    }
}

void KeepCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        if (token->kind == 0xFFFF && token->type == 4)
        {
            switch (token->value)
            {
            case 0x52:
                flags.raw |= 0x1;
                break;
            case 0x53:
                flags.raw |= 0x2;
                break;
            case 0x54:
                flags.raw |= 0x4;
                break;
            case 0x3C:
                flags.raw |= 0x8;
                break;
            case 0x1C:
                flags.raw |= 0x10;
                break;
            case 0xB2:
                flags.raw |= 0x20;
                break;
            case 0x55:
                flags.raw &= ~0x3F;
                break;
            default:
                break;
            }
        }

        reader.Next();
    }
}

void StoreCurrentSpaceCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->kind)
        {
        case 0x6:
            targetAndSpace = (targetAndSpace & ~0xFFu) | (token->value & 0xFF);
            break;
        case 0x7:
            targetAndSpace = (targetAndSpace & ~0xFF00u) | (token->value & 0xFF) << 8;
            break;
        case 0xFFFF:
            // The focus object's designator
            if (token->type == 4 && token->value == 0x1C)
            {
                targetAndSpace = (targetAndSpace & ~0xFF00u) | 0xFB << 8;
            }

            break;
        default:
            break;
        }

        reader.Next();
    }
}

void DestroyMeCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        if (token->kind == 0xFFFF && token->type == 4)
        {
            switch (token->value)
            {
            case 0x60:
                mode = (mode & ~0x7u) | 0x1;
                break;
            case 0x6D:
                mode = (mode & ~0x7u) | 0x2;
                break;
            case 0x5F:
                mode = (mode & ~0x38u) | 0x8;
                break;
            case 0x5E:
                mode = (mode & ~0x38u) | 0x10;
                break;
            default:
                break;
            }
        }

        reader.Next();
    }
}

void SetObjectCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->kind)
        {
        case 0x7E:
            flags = (flags & ~0x3u) | (TokenIsZero(token) ? 1 : 2);
            break;
        case 0x7F:
            flags = (flags & ~0xCu) | (TokenIsZero(token) ? 1 : 2) << 2;
            break;
        case 0x80:
            flags = (flags & ~0x30u) | (TokenIsZero(token) ? 1 : 2) << 4;
            break;
        case 0x81:
            flags = (flags & ~0xC0u) | (TokenIsZero(token) ? 1 : 2) << 6;
            break;
        case 0x86:
            flags = (flags & ~0x300u) | (TokenIsZero(token) ? 1 : 2) << 8;
            break;
        case 0x120:
            flags = (flags & ~0xC00u) | (TokenIsZero(token) ? 1 : 2) << 10;
            break;
        case 0x121:
            flags2 = (flags2 & ~0x3u) | (TokenIsZero(token) ? 1 : 2);
            break;
        case 0x12D:
            flags2 = (flags2 & ~0xCu) | (TokenIsZero(token) ? 1 : 2) << 2;
            break;
        case 0x138:
            flags2 = (flags2 & ~0x30u) | (TokenIsZero(token) ? 1 : 2) << 4;
            break;
        case 0x9B:
            flags |= 0x40000000 | 0x20000000;
            flags = (flags & ~0x1FE00000u) | (token->value & 0xFF) << 21;
            flags = (flags & ~0x1FE000u) | (token->value & 0xFF) << 13;
            break;
        case 0xCA:
            // Bits 21-28 and then, falling through, bits 13-20: the same as 0x9B
            flags |= 0x40000000;
            flags = (flags & ~0x1FE00000u) | (token->value & 0xFF) << 21;
            [[fallthrough]];
        case 0xC9:
            flags |= 0x20000000;
            flags = (flags & ~0x1FE000u) | (token->value & 0xFF) << 13;
            break;
        default:
            break;
        }

        reader.Next();
    }
}

// The commands without arguments read nothing
void ReleasePlayerHoldCommand::ParseTokens(const ScriptTokenList*)
{
}

void ResetCharacterFallCommand::ParseTokens(const ScriptTokenList*)
{
}

void WarpToChunkLinkTowardsPlayerCommand::ParseTokens(const ScriptTokenList*)
{
}

void SetCharacterHomeChunkCommand::ParseTokens(const ScriptTokenList*)
{
}

void GameControllerOp612Command::ParseTokens(const ScriptTokenList*)
{
}

void ClearPlayerFlag14Command::ParseTokens(const ScriptTokenList*)
{
}

void DisablePlayerControlCommand::ParseTokens(const ScriptTokenList*)
{
}

void ClearBottomTextCommand::ParseTokens(const ScriptTokenList*)
{
}

void ClearNodeValue174Command::ParseTokens(const ScriptTokenList*)
{
}

void CameraSaveParamsCommand::ParseTokens(const ScriptTokenList*)
{
}

void ResetMaskControllerCommand::ParseTokens(const ScriptTokenList*)
{
}

void CameraFocusObjectCommand::ParseTokens(const ScriptTokenList*)
{
}

void CameraStopFocusObjectCommand::ParseTokens(const ScriptTokenList*)
{
}

void DUMMY_FuelPayGateCommand::ParseTokens(const ScriptTokenList*)
{
}

void DUMMY_536Command::ParseTokens(const ScriptTokenList*)
{
}

void SetVehicleWrestleCreatureCommand::ParseTokens(const ScriptTokenList*)
{
}

void LinkedObjectNearestPlayerOp637Command::ParseTokens(const ScriptTokenList*)
{
}
