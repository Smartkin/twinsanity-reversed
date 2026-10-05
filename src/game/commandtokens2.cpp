#include "game/commands.h"

#include "game/math.h"
#include "game/objectnode.h"
#include "game/progress.h"
#include "game/properties.h"
#include "game/scripttokens.h"

// More of the commands' development tools parsers (their vtables' slot 2, game/commandtokens.cpp has the others): each token's
// tag names an argument, its value goes into the command's field. The retail game never calls them

namespace
{
// The vtable's parser of one token, which the commands sharing a class have each of their own
constexpr u32 ParseTokenSlot = 7;

// Every bit of what Keep keeps (KeepFlags')
constexpr u32 EveryKept = 0x3F;

// A priority above the most: the starter's own (SetBehaviourPriority's execution)
constexpr u32 StarterPriority = 0xFF;

// A setting a token gives: On gives it on, Off gives it off, any other value neither
void GiveSetting(GivenSettings* settings, const ScriptToken* token, u32 bit)
{
    if (token->value == KeywordOn)
    {
        settings->given |= bit;
        settings->values |= bit;
    }
    else if (token->value == KeywordOff)
    {
        settings->given |= bit;
        settings->values &= ~bit;
    }
}

// A tagged value's type set, the rest of it kept
void SetTaggedType(TaggedValue* value, TaggedValue::Type type)
{
    value->type = type;
}
}

void FinalBossWeaponsCommand::ParseTokens(const ScriptTokenList* tokens)
{
    // The mode tells the commands sharing the class apart, each with arguments of its own
    ScriptTokenReader reader;
    switch (mode)
    {
    case ModeHookJoints:
        ScriptTokenReader::Construct(&reader, tokens);
        reader.First();
        while (!reader.AtEnd())
        {
            const ScriptToken* token = reader.Current();
            switch (token->tag)
            {
            case TagFirstWeaponJoint:
                joints[0] = static_cast<u8>(token->value);
                break;
            case TagSecondWeaponJoint:
                joints[1] = static_cast<u8>(token->value);
                break;
            case TagThirdWeaponJoint:
                joints[2] = static_cast<u8>(token->value);
                break;
            default:
                break;
            }

            reader.Next();
        }

        break;
    case ModeReturn:
    case ModeRaise:
        ScriptTokenReader::Construct(&reader, tokens);
        reader.First();
        while (!reader.AtEnd())
        {
            const ScriptToken* token = reader.Current();
            if (token->tag == TagNone)
            {
                switch (token->value)
                {
                case KeywordFirstWeapon:
                    weapons.first = 1;
                    break;
                case KeywordSecondWeapon:
                    weapons.second = 1;
                    break;
                case KeywordThirdWeapon:
                    weapons.third = 1;
                    break;
                default:
                    break;
                }
            }

            reader.Next();
        }

        break;
    case ModeTarget:
        ScriptTokenReader::Construct(&reader, tokens);
        reader.First();
        while (!reader.AtEnd())
        {
            const ScriptToken* token = reader.Current();
            if (token->tag == TagSourceDesignator)
            {
                target.designator = TokenDesignator(token, target.designator);
            }

            reader.Next();
        }

        break;
    case ModeScaleAndRate:
        ScriptTokenReader::Construct(&reader, tokens);
        reader.First();
        while (!reader.AtEnd())
        {
            const ScriptToken* token = reader.Current();
            switch (token->tag)
            {
            case TagNone:
                switch (token->value)
                {
                case KeywordFirstWeapon:
                    weapons.first = 1;
                    break;
                case KeywordSecondWeapon:
                    weapons.second = 1;
                    break;
                case KeywordThirdWeapon:
                    weapons.third = 1;
                    break;
                default:
                    break;
                }

                break;
            case TagScale:
                scale = token->Float();
                break;
            case TagRate:
                turnRate = token->Float();
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
        switch (token->tag)
        {
        case TagX:
            SetTaggedType(&offsetX, TaggedValue::TypeFloat);
            ParseTaggedValueRecord(token, &offsetX);
            break;
        case TagY:
            SetTaggedType(&offsetY, TaggedValue::TypeFloat);
            ParseTaggedValueRecord(token, &offsetY);
            break;
        case TagZ:
            SetTaggedType(&offsetZ, TaggedValue::TypeFloat);
            ParseTaggedValueRecord(token, &offsetZ);
            break;
        case TagAcceleration:
            SetTaggedType(&pull, TaggedValue::TypeFloat);
            ParseTaggedValueRecord(token, &pull);
            break;
        case TagHomingPower:
            SetTaggedType(&turnRate, TaggedValue::TypeFloat);
            ParseTaggedValueRecord(token, &turnRate);
            break;
        case TagUnused13D:
            SetTaggedType(&unusedValue, TaggedValue::TypeFloat);
            ParseTaggedValueRecord(token, &unusedValue);
            break;
        case TagDrop:
            SetTaggedType(&drop, TaggedValue::TypeFloat);
            ParseTaggedValueRecord(token, &drop);
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
        switch (token->tag)
        {
        case TagParticle:
        {
            // Past the twelfth, the IDs go into the sounds
            u32 count = counts.ids;
            counts.ids = count + 1;
            u16* given = ids;
            given[count] = static_cast<u16>(token->value);
            break;
        }
        case TagSound:
        {
            u32 count = counts.sounds;
            counts.sounds = count + 1;
            u16* slots = soundSlots;
            slots[count] = static_cast<u16>(token->value);
            break;
        }
        case TagNone:
            if (token->value == KeywordAddAgain)
            {
                counts.addsAgain = 1;
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
        switch (token->tag)
        {
        case TagRotation:
            GiveSetting(&settings, token, 1u << SettingTurn);
            break;
        case TagForwardMovement:
            GiveSetting(&settings, token, 1u << SettingMoveZ);
            break;
        case TagLateralMovement:
            GiveSetting(&settings, token, 1u << SettingMoveX);
            break;
        case TagJumpKey:
            GiveSetting(&settings, token, 1u << SettingCross);
            break;
        case TagCrouchKey:
            GiveSetting(&settings, token, 1u << SettingSquare);
            break;
        case TagSpinKey:
            GiveSetting(&settings, token, 1u << SettingCircle);
            break;
        case TagInput:
            GiveSetting(&settings, token, 1u << SettingAllInputs);
            break;
        case TagAffectedByGravity:
            GiveSetting(&settings, token, 1u << SettingResting);
            break;
        case TagVulnerable:
            GiveSetting(&settings, token, 1u << SettingVulnerable);
            break;
        case TagUpdate:
            controls.boxOnly = !TokenIsOn(token);
            break;
        case TagNone:
            if (token->value == KeywordPadDriven)
            {
                controls.motionDriven = 0;
            }
            else if (token->value == KeywordMotionDriven)
            {
                controls.motionDriven = 1;
            }

            break;
        default:
            break;
        }

        reader.Next();
    }
}

void CountPlayerCirclingCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->tag)
        {
        case TagCounter:
            counter.counter = token->value;
            break;
        case TagAgentCounter:
            counter.counter = token->value;
            counter.agentCounter = 1;
            break;
        case TagNone:
            if (token->value == KeywordToTheRight)
            {
                counter.toTheRight = 1;
            }
            else if (token->value == KeywordToTheLeft)
            {
                counter.toTheRight = 0;
            }

            break;
        default:
            break;
        }

        reader.Next();
    }
}

void SetPlayerScriptFlagCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        if (token->tag == TagNone && token->type == TokenKeyword)
        {
            if (token->value == KeywordOn)
            {
                clears.on = 1;
            }
            else if (token->value == KeywordOff)
            {
                clears.on = 0;
            }
        }

        reader.Next();
    }
}

void MakeCharactersIdleCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        if (token->tag == TagNone && token->type == TokenKeyword)
        {
            switch (token->value)
            {
            case KeywordAll:
                characters.every = 1;
                break;
            case KeywordCrash:
                characters.crash = 1;
                break;
            case KeywordCortex:
                characters.cortex = 1;
                break;
            case KeywordMecha:
                characters.mecha = 1;
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
        switch (token->tag)
        {
        case TagLinked:
            target.linked = token->value - 1;
            target.byLinked = 1;
            break;
        case TagNone:
            if (token->type == TokenKeyword)
            {
                if (token->value == KeywordFocus)
                {
                    target.designator = DesignatesFocus;
                }
                else if (token->value == KeywordAgentRef1)
                {
                    target.designator = DesignatesAgentRef1;
                }
                else
                {
                    character = static_cast<s32>(TokenCharacter(token->value));
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
        switch (token->tag)
        {
        case TagActive:
            GiveSetting(&settings, token, 1u << SettingAwake);
            break;
        case TagVisible:
            GiveSetting(&settings, token, 1u << SettingVisible);
            break;
        case TagCollidable:
            GiveSetting(&settings, token, 1u << SettingCollision);
            break;
        case TagTangible:
            GiveSetting(&settings, token, 1u << SettingTriggerSignals);
            break;
        case TagShadow:
            GiveSetting(&settings, token, 1u << SettingShadow);
            break;
        case TagClamping:
            GiveSetting(&settings, token, 1u << SettingSnapsToGround);
            break;
        case TagHarmful:
            GiveSetting(&settings, token, 1u << SettingCanDamageCharacter);
            break;
        case TagVulnerable:
            GiveSetting(&settings, token, 1u << SettingVulnerable);
            break;
        case TagBulletsBounceBack:
            GiveSetting(&settings, token, 1u << SettingBulletsBounceBack);
            break;
        case TagTargettable:
            GiveSetting(&settings, token, 1u << SettingTargettable);
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
        if (token->tag == TagScale)
        {
            ParseTaggedValueRecord(token, &scale);
        }

        reader.Next();
    }
}

void StopStickingCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        if (token->tag == TagNone && token->type == TokenKeyword && token->value == KeywordKeep)
        {
            keepsStuck.on = 1;
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
        if (token->tag == TagHitPoints)
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
        if (token->tag == TagHitPoints)
        {
            hitPoints = token->value;
        }

        reader.Next();
    }
}

void SetCountedValueCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        if (token->tag == TagCountedValue)
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
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->tag)
        {
        case TagElevation:
            SetTaggedType(&pitch, TaggedValue::TypeAngle);
            ParseTaggedValueRecord(token, &pitch);
            given.pitch = 1;
            break;
        case TagZoom:
            given.distance = 1;
            distance = token->Float();
            break;
        default:
            break;
        }

        reader.Next();
    }
}

void SetPlayAreaCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->tag)
        {
        case TagValue:
            SetTaggedType(&areaValue, TaggedValue::TypeInt);
            ParseTaggedValueRecord(token, &areaValue);
            break;
        case TagNone:
            area = TokenArea(token->value);
            break;
        default:
            break;
        }

        reader.Next();
    }
}

void RaiseStoryAreaCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->tag)
        {
        case TagValue:
            SetTaggedType(&taggedArea, TaggedValue::TypeInt);
            ParseTaggedValueRecord(token, &taggedArea);
            break;
        case TagNone:
            area = TokenArea(token->value);
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
        if (token->tag == TagDuration)
        {
            seconds = token->Float();
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
        if (token->tag == TagDuration)
        {
            seconds = token->Float();
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
        switch (token->tag)
        {
        case TagAnimation:
            iconSlot = static_cast<s32>(token->value);
            break;
        case TagHitPoints:
            ParseTaggedValueRecord(token, &health);
            break;
        case TagWidth:
            barLength = token->Float();
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
        if (token->tag == TagNone)
        {
            healthChange = static_cast<s32>(token->value);
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
        switch (token->tag)
        {
        case TagAnimation:
            iconSlot = static_cast<s32>(token->value);
            break;
        case TagDuration:
            ParseTaggedValueRecord(token, &seconds);
            break;
        case TagHitPoints:
            ParseTaggedValueRecord(token, &total);
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
        if (token->tag == TagNone)
        {
            countChange = static_cast<s32>(token->value);
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
        if (token->tag == TagDuration)
        {
            seconds = token->Float();
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
        if (token->tag == TagDuration)
        {
            seconds = token->Float();
        }

        reader.Next();
    }
}

void SetGaugeIconCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        if (token->tag == TagAnimation)
        {
            slot.index = token->value;
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
        if (token->tag == TagCharacter)
        {
            character.character = TokenCharacter(token->value);
        }

        reader.Next();
    }
}

void NoOp586Command::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        if (token->tag == TagText)
        {
            unused1.low = token->value;
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
        if (token->tag == TagParticle)
        {
            // A retail bug: the count goes up to 15, and past the twelfth the IDs overwrite the count and then what follows the
            // command
            u32 index = count.ids;
            count.ids = index + 1;
            u16* given = ids;
            given[index] = static_cast<u16>(token->value);
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
        if (token->tag == TagNone)
        {
            gem = TokenGem(token->value);
        }

        reader.Next();
    }
}

void PickUpWumpaCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        if (token->tag == TagQuantity)
        {
            wumpaFruit = static_cast<s32>(token->value);
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
        switch (token->tag)
        {
        case TagRadius:
            respawn.unused0 = token->value == KeywordOn ? 1u : 0u;
            break;
        case TagSaves:
            respawn.saves = token->value == KeywordOn ? 1u : 0u;
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
        if (token->tag == TagBecome)
        {
            if (token->value == KeywordMaster)
            {
                link.focusLeads = 0;
            }
            else if (token->value == KeywordSlave)
            {
                link.focusLeads = 1;
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
        if (token->tag == TagLives)
        {
            lives = static_cast<s32>(token->value);
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
        if (token->tag == TagCharacter)
        {
            character = static_cast<s32>(TokenCharacter(token->value));
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
        if (token->tag == TagCharacter)
        {
            character = static_cast<s32>(TokenCharacter(token->value));
        }

        reader.Next();
    }
}

void PushPlayerVehicleCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        if (token->tag == TagNone && token->type == TokenFloat)
        {
            push = token->Float();
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
        if (token->tag == TagRedWumpaCount)
        {
            wumpaFruit = token->value;
        }

        reader.Next();
    }
}

void ApplyVelocityToHeldBodyCommand::ParseTokens(const ScriptTokenList* tokens)
{
    // The impulse starts as the zero point (w 1)
    impulse = g_DefaultBox.min;
    impulse.w = 1.0f;
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->tag)
        {
        case TagVelocityX:
            impulse.x = token->Float();
            break;
        case TagVelocityY:
            impulse.y = token->Float();
            break;
        case TagVelocityZ:
            impulse.z = token->Float();
            break;
        default:
            break;
        }

        reader.Next();
    }
}

void SetCustomPickupCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->tag)
        {
        case TagHitCrates:
            flags.unused0 = TokenIsOn2(token);
            break;
        case TagHitCreatures:
            flags.unused1 = TokenIsOn2(token);
            break;
        case TagHitFurniture:
            flags.unused2 = TokenIsOn2(token);
            break;
        case TagHitPlayer:
            flags.unused3 = TokenIsOn2(token);
            break;
        case TagHitAgents:
            flags.unused4 = TokenIsOn2(token);
            break;
        case TagHitAll:
            flags.unused4 = TokenIsOn2(token);
            flags.unused5 = TokenIsOn2(token);
            break;
        case TagHitScenery:
            flags.unused5 = TokenIsOn2(token);
            break;
        case TagWumpaHover:
            flags.spins = TokenIsOn2(token);
            break;
        case TagChainSuck:
            flags.unused7 = TokenIsOn2(token);
            break;
        case TagHitPoints:
            flags.unused8 = token->value;
            break;
        case TagSuckRange:
            radius = token->Float();
            flags.unused10 = 1;
            break;
        case TagSuckPower:
            pull = token->Float();
            flags.unused10 = 1;
            break;
        case TagFlySpeed:
            fleeSpeed = token->Float();
            break;
        default:
            break;
        }

        reader.Next();
    }
}

void SetCustomProjectileCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->tag)
        {
        case TagFlySpeed:
            speed = token->Float();
            break;
        case TagFlatMultiplier:
            sideTurnScale = token->Float();
            break;
        case TagHomingPower:
            settings.homes = 1;
            turn = token->Float();
            break;
        case TagTurnLimit:
            settings.homesForATime = 1;
            homingTime = token->Float();
            break;
        case TagGravity:
            ParseTaggedValueRecord(token, &gravity);
            settings.falls = 1;
            break;
        case TagHitPoints:
            settings.unused0 = token->value;
            break;
        case TagNone:
            if (token->value == KeywordPlayersShot)
            {
                settings.playersShot = 1;
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
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        TokenVectorComponent(token, &offset.x);
        switch (token->tag)
        {
        case TagChildActor:
            object.id = token->value;
            break;
        case TagMessage:
            object.message = token->value;
            break;
        case TagExitPoint:
            shot.exitPoint = token->value;
            break;
        case TagMetresPerSecond:
            shot.speedGiven = 1;
            speed = token->Float();
            break;
        case TagNone:
            if (token->type == TokenKeyword)
            {
                switch (token->value)
                {
                case KeywordUp:
                    shot.unused8 = 0;
                    break;
                case KeywordDown:
                    shot.unused8 = 1;
                    break;
                case KeywordForward:
                    shot.unused8 = 2;
                    break;
                case KeywordBack:
                    shot.unused8 = 3;
                    break;
                case KeywordLeft:
                    shot.unused8 = 4;
                    break;
                case KeywordRight:
                    shot.unused8 = 5;
                    break;
                case KeywordOrient:
                    shot.unused12 = 1;
                    break;
                case KeywordPassAgentRef1:
                    shot.atAgentRef1 = 1;
                    break;
                case KeywordNoBounce:
                    shot.noBounce = 1;
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

    if (!(__builtin_fabsf(offset.x) <= Epsilon && __builtin_fabsf(offset.y) <= Epsilon && __builtin_fabsf(offset.z) <= Epsilon))
    {
        shot.offsetGiven = 1;
    }
}

void NoOp568Command::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->tag)
        {
        case TagMetresPerSecond:
            unused1 = token->Float();
            break;
        case TagUnusedA2:
            unused2 = token->Float();
            break;
        case TagUnusedA3:
            unused3 = token->Float();
            break;
        case TagTurn:
            unused4 = token->Float();
            break;
        case TagTurnLimit:
            unused5 = token->Float();
            break;
        case TagUnused231:
            unused6.low = token->value;
            break;
        case TagUnused232:
            unused6.high = token->value;
            break;
        case TagUnused233:
            unused7.low = token->value;
            break;
        case TagUnused235:
            unused7.high = token->value;
            break;
        case TagUnused234:
            unused8.low = token->value;
            break;
        default:
            break;
        }

        reader.Next();
    }
}

void SetVehicleHumiliskateCommand::ParseTokens(const ScriptTokenList* tokens)
{
    // These speeds unless the tokens give others
    constexpr f32 DefaultTopSpeed = 25.0f;
    constexpr f32 DefaultCrouchedSpeed = 35.0f;
    SetTaggedType(&topSpeed, TaggedValue::TypeFloat);
    topSpeed.SetFloat(DefaultTopSpeed);
    SetTaggedType(&crouchedSpeed, TaggedValue::TypeFloat);
    crouchedSpeed.SetFloat(DefaultCrouchedSpeed);
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->tag)
        {
        case TagTopSpeed:
            ParseTaggedValueRecord(token, &topSpeed);
            break;
        case TagCrouchedSpeed:
            ParseTaggedValueRecord(token, &crouchedSpeed);
            break;
        case TagFirstCharacter:
            characters.first = TokenCharacter(token->value);
            break;
        case TagSecondCharacter:
            characters.second = TokenCharacter(token->value);
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
        switch (token->tag)
        {
        case TagAgent:
            rider.receiver = token->value;
            break;
        case TagNone:
            if (token->value == KeywordBoardControls)
            {
                rider.boardControls = 1;
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
        switch (token->tag)
        {
        case TagRange:
        case TagAvoidFocus:
            block.halfWidth = token->Float();
            break;
        case TagHalfHeight:
            block.halfHeight = token->Float();
            break;
        case TagNearFocus:
            block.exposureWeight = token->Float();
            break;
        case TagDistanceWeight:
            block.distanceWeight = token->Float();
            break;
        case TagNone:
            if (token->type == TokenKeyword && token->value == KeywordSearchInBox)
            {
                block.search.kind = MotionBlock::SearchInBox;
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
        switch (token->tag)
        {
        case TagRangeFirst:
            range.first = token->value;
            break;
        case TagRangeEnd:
            range.end = token->value;
            break;
        default:
            break;
        }

        reader.Next();
    }
}

void NoOpNowGoForwardCollidableCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ParseTaggedValueTokens(tokens, &unused1);
}

void AttachAllLinkedAgentsCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        if (token->tag == TagExitPoint)
        {
            settings.exitPointGiven = 1;
            settings.exitPoint = token->value;
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
        switch (token->tag)
        {
        case TagFirstCharacter:
            characters.first = TokenCharacter(token->value);
            break;
        case TagSecondCharacter:
            characters.second = TokenCharacter(token->value);
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
        if (token->tag == TagFirstCharacter)
        {
            character = static_cast<s32>(TokenCharacter(token->value));
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
        if (token->tag == TagQuantity)
        {
            ammo = static_cast<s32>(token->value);
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
        switch (token->tag)
        {
        case TagPriority:
            if (token->type != TokenKeyword)
            {
                priority.priority = token->value;
            }
            else if (token->value == KeywordDefault)
            {
                priority.priority = StarterPriority;
            }

            break;
        case TagNone:
            if (token->type == TokenKeyword && token->value == KeywordDefault)
            {
                priority.priority = StarterPriority;
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
        if (token->tag == TagNone && token->type == TokenKeyword)
        {
            switch (token->value)
            {
            case KeywordTrail:
                keeps.particles = 1;
                break;
            case KeywordWobble:
                keeps.trajectory = 1;
                break;
            case KeywordCollider:
                keeps.unused2 = 1;
                break;
            case KeywordAttach:
                keeps.unused3 = 1;
                break;
            case KeywordFocus:
                keeps.unused4 = 1;
                break;
            case KeywordPerception:
                keeps.perception = 1;
                break;
            case KeywordNothing:
                keeps.value &= ~EveryKept;
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
        switch (token->tag)
        {
        case TagAgent:
            request.receiver = token->value;
            break;
        case TagKey:
            request.designator = token->value;
            break;
        case TagNone:
            // The focus object's designator
            if (token->type == TokenKeyword && token->value == KeywordFocus)
            {
                request.designator = DesignatesFocus;
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
        if (token->tag == TagNone && token->type == TokenKeyword)
        {
            switch (token->value)
            {
            case KeywordHard:
                settings.mode = 1;
                break;
            case KeywordForever:
                settings.mode = 2;
                break;
            case KeywordNodal:
                settings.unused3 = 1;
                break;
            case KeywordZonal:
                settings.unused3 = 2;
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
        switch (token->tag)
        {
        case TagBusy:
            switches.busy = TokenIsOn(token) ? SwitchOn : SwitchOff;
            break;
        case TagRender:
            switches.visible = TokenIsOn(token) ? SwitchOn : SwitchOff;
            break;
        case TagCollide:
            switches.collision = TokenIsOn(token) ? SwitchOn : SwitchOff;
            break;
        case TagUpdate:
            switches.asleep = TokenIsOn(token) ? SwitchOn : SwitchOff;
            break;
        case TagLandable:
            switches.unused8 = TokenIsOn(token) ? SwitchOn : SwitchOff;
            break;
        case TagPushable:
            switches.pushable20 = TokenIsOn(token) ? SwitchOn : SwitchOff;
            break;
        case TagPushableToo:
            bodySwitches.pushable40 = TokenIsOn(token) ? SwitchOn : SwitchOff;
            break;
        case TagCarries:
            bodySwitches.carries = TokenIsOn(token) ? SwitchOn : SwitchOff;
            break;
        case TagSolid:
            bodySwitches.solid = TokenIsOn(token) ? SwitchOn : SwitchOff;
            break;
        case TagLodBoost:
            switches.nodeDistanceGiven = 1;
            switches.modelDistanceGiven = 1;
            switches.nodeDistance = token->value;
            switches.modelDistance = token->value;
            break;
        case TagNodeDistance:
            // The node's distance and then, falling through, the model's: the same as TagLodBoost
            switches.nodeDistanceGiven = 1;
            switches.nodeDistance = token->value;
            [[fallthrough]];
        case TagModelDistance:
            switches.modelDistanceGiven = 1;
            switches.modelDistance = token->value;
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

void EnablePlayerControlCommand::ParseTokens(const ScriptTokenList*)
{
}

void ClearCharacterDeadCommand::ParseTokens(const ScriptTokenList*)
{
}

void DisablePlayerControlCommand::ParseTokens(const ScriptTokenList*)
{
}

void ClearBottomTextCommand::ParseTokens(const ScriptTokenList*)
{
}

void ClearCountedValueCommand::ParseTokens(const ScriptTokenList*)
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

void NoOpFuelPayGateCommand::ParseTokens(const ScriptTokenList*)
{
}

void NoOp536Command::ParseTokens(const ScriptTokenList*)
{
}

void SetVehicleWrestleCreatureCommand::ParseTokens(const ScriptTokenList*)
{
}

void SetFocusToLinkedObjectInViewCommand::ParseTokens(const ScriptTokenList*)
{
}
