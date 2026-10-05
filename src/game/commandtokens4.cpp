#include "game/commands.h"

#include "game/collision.h"
#include "game/math.h"
#include "game/objectnode.h"
#include "game/objects.h"
#include "game/progress.h"
#include "game/scripttokens.h"
#include "game/shadows.h"
#include "game/sound.h"

// More of the commands' development tools parsers (game/commandtokens.cpp): each token's tag names an argument, its value goes
// into the command's field. The retail game never calls them

namespace
{
// The bits of a switch argument SetRestartable's parser writes (the whole low byte)
constexpr u32 SwitchByte = 0xFF;

// The smallest scale QueueObjectVideo's parser takes
constexpr f32 SmallestVideoScale = Rounded(0.01);

// A tagged value made a float or an integer and given the token's value
void ParseTaggedFloat(const ScriptToken* token, TaggedValue* value)
{
    value->type = TaggedValue::TypeFloat;
    ParseTaggedValueRecord(token, value);
}

void ParseTaggedInt(const ScriptToken* token, TaggedValue* value)
{
    value->type = TaggedValue::TypeInt;
    ParseTaggedValueRecord(token, value);
}

// The cycle an axis of a motion block follows that a keyword names (MotionBlock::Cycle; the tool's triangle is the game's second
// square wave), 0 none
u32 WobbleWave(u32 keyword)
{
    switch (keyword)
    {
    case KeywordSine:
        return MotionBlock::CycleSine;
    case KeywordSquare:
        return MotionBlock::CycleSquare;
    case KeywordTriangle:
        return MotionBlock::CycleSquareToo;
    case KeywordRandom:
        return MotionBlock::CycleRandom;
    case KeywordSpin:
        return MotionBlock::CycleAngle;
    default:
        return 0;
    }
}

// The kind of sense a keyword names (PerceptionSense::Kind), -1 none
s32 SenseKindOf(u32 keyword)
{
    switch (keyword)
    {
    case KeywordSenseInstances:
        return PerceptionSense::KindInstances;
    case KeywordSenseRising:
        return PerceptionSense::KindRising;
    case KeywordSenseSpeed:
        return PerceptionSense::KindSpeed;
    case KeywordUnusedSense:
        return PerceptionSense::KindUnused;
    default:
        return -1;
    }
}
}

void SetWobbleCommand::ParseTokens(const ScriptTokenList* tokens)
{
    // In picks, for a sine, the phase passing the start falling
    f32 duration = 0.0f;
    f32 startX = 0.0f;
    f32 startY = 0.0f;
    f32 startZ = 0.0f;
    u32 fallingX = 0;
    u32 fallingY = 0;
    u32 fallingZ = 0;
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->tag)
        {
        case TagDuration:
            duration = token->Float();
            break;
        case TagXWave:
        {
            u32 wave = WobbleWave(token->value);
            if (wave != 0)
            {
                block.motion.cycleX = wave;
                block.flags.cyclesAboutX = 1;
            }

            break;
        }
        case TagYWave:
        {
            u32 wave = WobbleWave(token->value);
            if (wave != 0)
            {
                block.motion.cycleY = wave;
                block.flags.cyclesAboutY = 1;
            }

            break;
        }
        case TagZWave:
        {
            u32 wave = WobbleWave(token->value);
            if (wave != 0)
            {
                block.motion.cycleZ = wave;
                block.flags.cyclesAboutZ = 1;
            }

            break;
        }
        case TagXMagnitude:
            block.cycleRanges[0] = token->Float();
            if (block.motion.cycleX == 0)
            {
                block.motion.cycleX = MotionBlock::CycleSine;
                block.flags.cyclesAboutX = 1;
            }

            break;
        case TagYMagnitude:
            block.cycleRanges[1] = token->Float();
            if (block.motion.cycleY == 0)
            {
                block.motion.cycleY = MotionBlock::CycleSine;
                block.flags.cyclesAboutY = 1;
            }

            break;
        case TagZMagnitude:
            block.cycleRanges[2] = token->Float();
            if (block.motion.cycleZ == 0)
            {
                block.motion.cycleZ = MotionBlock::CycleSine;
                block.flags.cyclesAboutZ = 1;
            }

            break;
        case TagXTime:
            block.cycleRates[0] = TwoPi / token->Float();
            break;
        case TagYTime:
            block.cycleRates[1] = TwoPi / token->Float();
            break;
        case TagZTime:
            block.cycleRates[2] = TwoPi / token->Float();
            break;
        case TagXPhase:
            block.cyclePhases[0] = token->Float() * DegreesToRadians;
            break;
        case TagYPhase:
            block.cyclePhases[1] = token->Float() * DegreesToRadians;
            break;
        case TagZPhase:
            block.cyclePhases[2] = token->Float() * DegreesToRadians;
            break;
        case TagXDisplacement:
            startX = token->Float();
            break;
        case TagYDisplacement:
            startY = token->Float();
            break;
        case TagZDisplacement:
            startZ = token->Float();
            break;
        case TagXWay:
            if (token->value == KeywordIn)
            {
                fallingX = 1;
            }

            break;
        case TagYWay:
            if (token->value == KeywordIn)
            {
                fallingY = 1;
            }

            break;
        case TagZWay:
            if (token->value == KeywordIn)
            {
                fallingZ = 1;
            }

            break;
        case TagRandomXPhase:
            block.motion.randomPhaseX = 1;
            block.cyclePhases[0] = token->Float() * DegreesToRadians;
            break;
        case TagRandomYPhase:
            block.motion.randomPhaseY = 1;
            block.cyclePhases[1] = token->Float() * DegreesToRadians;
            break;
        case TagRandomZPhase:
            block.motion.randomPhaseZ = 1;
            block.cyclePhases[2] = token->Float() * DegreesToRadians;
            break;
        case TagJoint:
            block.mover = static_cast<u8>(token->value);
            break;
        case TagXClip:
            block.motion.signX = token->value == KeywordPositive ? MotionBlock::SignPositive : MotionBlock::SignNegative;
            break;
        case TagYClip:
            block.motion.signY = token->value == KeywordPositive ? MotionBlock::SignPositive : MotionBlock::SignNegative;
            break;
        case TagZClip:
            block.motion.signZ = token->value == KeywordPositive ? MotionBlock::SignPositive : MotionBlock::SignNegative;
            break;
        case TagSyncTo:
            if (token->value == KeywordFocus)
            {
                block.flags.cycleSource = FocusCycles;
            }
            else if (token->value == KeywordFirst)
            {
                block.flags.cycleSource = AttachedCycles;
            }

            break;
        case TagScrew:
            block.spinDegrees = token->Float();
            block.motion.roll = MotionBlock::RollSpin;
            break;
        case TagNone:
            if (token->type == TokenKeyword)
            {
                switch (token->value)
                {
                case KeywordWorldSpace:
                    block.motion.space = ControlPacket::WorldSpace;
                    break;
                case KeywordInitialSpace:
                    block.motion.space = SpaceStart;
                    break;
                case KeywordCurrentSpace:
                    block.motion.space = SpaceOwn;
                    break;
                case KeywordTrackedSpace:
                    block.motion.space = SpaceTracked;
                    break;
                case KeywordInitialPosition:
                    block.motion.space = ControlPacket::InitialPosition;
                    break;
                case KeywordStoredSpace:
                    block.motion.space = SpaceStored;
                    break;
                case KeywordAngular:
                    block.motion.turns = 1;
                    break;
                case KeywordOrient:
                    block.motion.facesMove = 1;
                    break;
                case KeywordScaleJoint:
                    block.motion.unused19 = 1;
                    break;
                case KeywordBall:
                    block.motion.roll = MotionBlock::RollY;
                    break;
                case KeywordWheel:
                    block.motion.roll = MotionBlock::RollX;
                    break;
                case KeywordOneWay:
                    block.flags.spinsBySize = 1;
                    break;
                case KeywordSmoothMagnitude:
                    block.flags.growsIn = 1;
                    break;
                case KeywordFaceTracked:
                    block.flags.facesTracked = 1;
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

    SetUpMotionBlock(&block, fallingX, fallingY, fallingZ, duration, startX, startY, startZ);
    block.flags.followedWhenTouched = 1;
}

void AlterWobblePhaseCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->tag)
        {
        case TagXPhase:
            ParseTaggedFloat(token, &phaseX);
            axes.x = 1;
            break;
        case TagYPhase:
            ParseTaggedFloat(token, &phaseY);
            axes.y = 1;
            break;
        case TagZPhase:
            ParseTaggedFloat(token, &phaseZ);
            axes.z = 1;
            break;
        default:
            break;
        }

        reader.Next();
    }
}

void AddWobblePhaseCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->tag)
        {
        case TagXPhase:
            ParseTaggedFloat(token, &turnX);
            axes.x = 1;
            break;
        case TagYPhase:
            ParseTaggedFloat(token, &turnY);
            axes.y = 1;
            break;
        case TagZPhase:
            ParseTaggedFloat(token, &turnZ);
            axes.z = 1;
            break;
        default:
            break;
        }

        reader.Next();
    }
}

void SetCycleAmplitudesCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->tag)
        {
        case TagAmplitudeX:
            ParseTaggedFloat(token, &amplitudeX);
            axes.x = 1;
            break;
        case TagAmplitudeY:
            ParseTaggedFloat(token, &amplitudeY);
            axes.y = 1;
            break;
        case TagAmplitudeZ:
            ParseTaggedFloat(token, &amplitudeZ);
            axes.z = 1;
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
        switch (token->tag)
        {
        case TagLoudness:
            weight = token->Float();
            break;
        case TagAgent:
            target.receiver = token->value;
            break;
        case TagNone:
            switch (token->value)
            {
            case KeywordPlayer:
                target.player = 1;
                break;
            case KeywordAgentRef2:
                target.agentRef2 = 1;
                break;
            case KeywordFocus:
                target.focus = 1;
                break;
            case KeywordDefault:
                target.remembers = 1;
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
        switch (token->tag)
        {
        case TagPerceptionWeight:
            weight = __builtin_bit_cast(f32, token->value);
            break;
        case TagNone:
        {
            s32 kind = SenseKindOf(token->value);
            if (kind >= 0)
            {
                sense = kind;
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
        switch (token->tag)
        {
        case TagPerceptionWeight:
            weight = __builtin_bit_cast(f32, token->value);
            break;
        case TagNone:
        {
            s32 kind = SenseKindOf(token->value);
            if (kind >= 0)
            {
                sense = kind;
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
        switch (token->tag)
        {
        case TagRate:
            strength = token->Float();
            break;
        case TagNone:
        {
            s32 kind = SenseKindOf(token->value);
            if (kind >= 0)
            {
                perceptionSlot = kind;
            }

            break;
        }
        default:
            break;
        }

        reader.Next();
    }
}

void TurnSenseOffCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        if (token->tag == TagNone)
        {
            s32 kind = SenseKindOf(token->value);
            if (kind >= 0)
            {
                sense = kind;
            }
        }

        reader.Next();
    }
}

void TurnSenseOnCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        if (token->tag == TagNone)
        {
            s32 kind = SenseKindOf(token->value);
            if (kind >= 0)
            {
                sense = kind;
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
        switch (token->tag)
        {
        case TagShadowSlot:
            slot.slot = token->value;
            break;
        case TagShadowDistance:
            ParseTaggedValueRecord(token, &distance);
            break;
        case TagNearStrength:
            ParseTaggedValueRecord(token, &nearStrength);
            break;
        case TagFarStrength:
            ParseTaggedValueRecord(token, &farStrength);
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
        switch (token->tag)
        {
        case TagShadowSlot:
            shape.slot = token->value;
            break;
        case TagNone:
            shape.kind = ShadowShapeOfToken(token->value);
            break;
        case TagJoint:
            shape.joint = token->value;
            break;
        case TagWidth:
            ParseTaggedValueRecord(token, &radius);
            break;
        case TagDepth:
            ParseTaggedValueRecord(token, &height);
            break;
        case TagSize:
            ParseTaggedValueRecord(token, &radius);
            height = radius;
            break;
        case TagXDisplacement:
            ParseTaggedValueRecord(token, &offsetX);
            break;
        case TagYDisplacement:
            ParseTaggedValueRecord(token, &offsetY);
            break;
        case TagZDisplacement:
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
        switch (token->tag)
        {
        case TagShadowSlot:
            shape.slot = token->value;
            break;
        case TagNone:
            shape.kind = ShadowShapeOfToken(token->value);
            break;
        case TagShadowJoint:
            shape.joint = token->value;
            break;
        case TagShadowSecondJoint:
            shape.secondJoint = token->value;
            break;
        case TagSize:
            ParseTaggedValueRecord(token, &radius);
            break;
        case TagXDisplacement:
            ParseTaggedValueRecord(token, &offsetX);
            break;
        case TagYDisplacement:
            ParseTaggedValueRecord(token, &offsetY);
            break;
        case TagZDisplacement:
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
        switch (token->tag)
        {
        case TagShadowSlot:
            shape.slot = token->value;
            break;
        case TagNone:
            shape.kind = ShadowShapeOfToken(token->value);
            break;
        case TagWidth:
            ParseTaggedValueRecord(token, &width);
            break;
        case TagDepth:
            ParseTaggedValueRecord(token, &depth);
            break;
        case TagSize:
            ParseTaggedValueRecord(token, &width);
            depth = width;
            break;
        case TagXDisplacement:
            ParseTaggedValueRecord(token, &offsetX);
            break;
        case TagYDisplacement:
            ParseTaggedValueRecord(token, &offsetY);
            break;
        case TagZDisplacement:
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
    soundSlots[0] = NoSoundSlot;
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->tag)
        {
        case TagSound:
        {
            // A list of the slots, its count in the flags (past the eighth, into the volume and on)
            u32 count = flags.slotCount;
            flags.slotCount = count + 1;
            u16* sounds = soundSlots;
            sounds[count] = static_cast<u16>(token->value);
            break;
        }
        case TagSoundBank:
            switch (token->value)
            {
            case KeywordFx1:
                flags.group = EffectsGroup;
                break;
            case KeywordFx2:
                flags.group = SecondEffectsGroup;
                break;
            case KeywordMusic:
                flags.group = MusicGroup;
                break;
            case KeywordSpeech:
                flags.group = MovieGroup;
                break;
            default:
                break;
            }

            break;
        case TagVolume:
            ParseTaggedFloat(token, &volume);
            flags.volumeGiven = 1;
            break;
        case TagRelativePitch:
            flags.pitchGiven = 1;
            pitch = token->Float();
            break;
        case TagRandomPitch:
            flags.randomPitch = 1;
            pitchRandom = token->Float();
            break;
        case TagRandomVolume:
            flags.randomVolume = 1;
            volumeRandom = token->Float();
            break;
        case TagShakeStrength:
            flags.shakes = 1;
            shakeStrength = token->Float();
            break;
        case TagShakeX:
            flags.shakes = 1;
            shakeAcross = token->Float();
            break;
        case TagShakeY:
            flags.shakes = 1;
            shakeUp = token->Float();
            break;
        case TagShakeFalloff:
            shakeFalloff = token->Float();
            break;
        case TagNone:
            if (token->type == TokenKeyword)
            {
                switch (token->value)
                {
                case KeywordCentral:
                    flags.unplaced = 1;
                    break;
                case KeywordTrail:
                    flags.followed = 1;
                    break;
                case KeywordLooped:
                    flags.tracked = 1;
                    break;
                case KeywordOnImpact:
                    flags.contactKind = ContactImpact;
                    break;
                case KeywordOnStep:
                    flags.contactKind = ContactStep1;
                    break;
                case KeywordOnOtherStep:
                    flags.contactKind = ContactStep2;
                    break;
                case KeywordOnLand:
                    flags.contactKind = ContactLand;
                    break;
                case KeywordOnHardImpact:
                    flags.contactKind = ContactHardImpact;
                    break;
                case KeywordOnScrape:
                    flags.contactKind = ContactScrape;
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
        switch (token->tag)
        {
        case TagTune:
            ParseTaggedInt(token, &track);
            break;
        case TagVolume:
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
        case TagBlendSeconds:
            fadeTime = token->Float();
            break;
        case TagRange:
            range = token->Float();
            break;
        case TagNone:
            if (token->type == TokenKeyword)
            {
                switch (token->value)
                {
                case KeywordArea:
                    flags.slot = MainMusicSlot;
                    break;
                case KeywordContext:
                    flags.slot = ContextMusicSlot;
                    break;
                case KeywordUngrouped:
                    flags.slot = SpareMusicSlot;
                    break;
                case KeywordEmitter:
                    flags.slot = BeginMusicFlags::EmitterSlot;
                    break;
                case KeywordOnce:
                    flags.loops = 0;
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

void FadeOutMusicSlotCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->tag)
        {
        case TagBlendSeconds:
            fadeSeconds = token->Float();
            break;
        case TagNone:
            if (token->type == TokenKeyword && token->value == KeywordUngrouped)
            {
                slot.index = SpareMusicSlot;
            }

            break;
        default:
            break;
        }

        reader.Next();
    }
}

void SetReverbCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->tag)
        {
        case TagDelay:
            reverb.delay = token->Float();
            reverb.bits.unused8 = 0.0f < reverb.delay ? 1 : 0;
            break;
        case TagFeedback:
            reverb.feedback = token->Float();
            reverb.bits.unused9 = 0.0f < reverb.feedback ? 1 : 0;
            break;
        case TagReverbDepth:
            reverb.depth = token->Float();
            break;
        case TagNone:
            if (token->type == TokenKeyword)
            {
                if (token->value >= KeywordFirstReverb && token->value <= KeywordLastReverb)
                {
                    reverb.bits.type = token->value - KeywordFirstReverb;
                }
                else if (token->value == KeywordBoxReverb)
                {
                    boxReverb.on = 1;
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
        switch (token->tag)
        {
        case TagTune:
            ParseTaggedInt(token, &track);
            break;
        case TagNone:
            if (token->type == TokenKeyword && token->value == KeywordOnce)
            {
                flags.loops = 0;
            }

            break;
        default:
            break;
        }

        reader.Next();
    }
}

void SetContactSoundsCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->tag)
        {
        case TagFirstContactSound:
            if (token->value < ObjectNode::NoContactSoundSlot)
            {
                slots.first = token->value;
            }

            break;
        case TagLastContactSound:
            if (token->value < ObjectNode::NoContactSoundSlot)
            {
                slots.last = token->value;
            }

            break;
        case TagNone:
            if (token->type == TokenKeyword && token->value == KeywordContactSoundsOff)
            {
                slots.off = 1;
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
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        flags.space = TokenSpace(token, flags.space);
        TokenVectorComponent(token, &offsetX);
        switch (token->tag)
        {
        case TagSubtype:
            flags.subtypeGiven = 1;
            ParseTaggedInt(token, &subtype);
            break;
        case TagExitPoint:
            flags.exitPoint = token->value;
            break;
        case TagKey:
            object.designator = token->value - 1;
            break;
        case TagMessage:
            flags.message = token->value;
            break;
        case TagLodBoost:
            object.nearDistance = token->value;
            break;
        case TagChildActor:
            object.id = token->value;
            break;
        case TagCopyState:
            flags.agentsId = TokenIsOn(token);
            break;
        case TagNone:
            if (token->type == TokenKeyword)
            {
                switch (token->value)
                {
                case KeywordCurrentKey:
                    object.designator = DesignatesCurrentKey;
                    break;
                case KeywordNextKey:
                    object.designator = DesignatesNextKey;
                    break;
                case KeywordInert:
                    flags.unsignalled = 1;
                    break;
                case KeywordAttach:
                    flags.atAgent = 1;
                    break;
                case KeywordMakeLinked:
                    flags.linksToAgent = 1;
                    break;
                case KeywordFocusPosition:
                    flags.atFocusPosition = 1;
                    break;
                case KeywordFocus:
                    flags.atFocus = 1;
                    break;
                case KeywordSpaceRotation:
                    flags.turnedBySpace = 1;
                    break;
                case KeywordSelf:
                    flags.ownObject = 1;
                    break;
                case KeywordMakeFocus:
                    flags.becomesFocus = 1;
                    break;
                case KeywordMakeAgentRef2:
                    flags.becomesAgentRef2 = 1;
                    break;
                case KeywordLinkSpawner:
                    flags.linksSpawner = 1;
                    break;
                case KeywordPassAgentRef1:
                    flags.sharesAgentRef1 = 1;
                    break;
                case KeywordShareAgentRef2:
                    flags.sharesAgentRef2 = 1;
                    break;
                case KeywordShareFocus:
                    flags.sharesFocus = 1;
                    break;
                case KeywordShareStoredPosition:
                    flags.sharesStoredPosition = 1;
                    break;
                case KeywordShareLinked:
                    flags.sharesLinked = 1;
                    break;
                case KeywordStoredPosition:
                    flags.atStoredPosition = 1;
                    break;
                case KeywordMakeBusy:
                    flags.marksBusy = 1;
                    break;
                case KeywordMakeIdle:
                    flags.clearsBusy = 1;
                    break;
                case KeywordShareKeys:
                    flags.sharesKeys = 1;
                    break;
                case KeywordSharePaths:
                    flags.sharesPaths = 1;
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
        flags.offsetGiven = 1;
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
        switch (token->tag)
        {
        case TagExitPoint:
            attachment.exitPoint = token->value;
            break;
        case TagNone:
            if (token->type == TokenKeyword && token->value == KeywordAll)
            {
                attachment.everyLinked = 1;
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

void StartObjectVideoCommand::ParseTokens(const ScriptTokenList*)
{
}

void CancelVideoCommand::ParseTokens(const ScriptTokenList*)
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
        switch (token->tag)
        {
        case TagBehaviourSlot:
            request.slot = token->value;
            break;
        case TagRunnerSlot:
            request.runner = token->value;
            break;
        default:
            break;
        }

        reader.Next();
    }
}

void SetRankCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        if (token->tag == TagRank)
        {
            ParseTaggedInt(token, &rank);
        }

        reader.Next();
    }
}

void SetTriggerRankCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        if (token->tag == TagRank)
        {
            ParseTaggedInt(token, &rank);
        }

        reader.Next();
    }
}

void SetRestartableCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        if (token->tag == TagRestartable)
        {
            restartable.value = (restartable.value & ~SwitchByte) | (token->value == KeywordOn ? 1u : 0u);
        }

        reader.Next();
    }
}

void CopyDesignatorCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->tag)
        {
        case TagSourceDesignator:
            designators.source = TokenDesignator(token, designators.source);
            break;
        case TagDestinationDesignator:
            designators.destination = TokenDesignator(token, designators.destination);
            break;
        case TagSourceAgent:
            designators.sourceAgent = TokenDesignator(token, designators.sourceAgent);
            break;
        default:
            break;
        }

        reader.Next();
    }
}

void QueueObjectVideoCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->tag)
        {
        case TagCutscene:
            cutscene = static_cast<s32>(token->value);
            break;
        case TagScale:
        {
            // A float of at least 0.01. Retail bug: a value above 1 is made -1, which the minimum then makes
            // 0.01 (probably meant to be 1)
            f32 scale = token->Float();
            if (1.0f < scale)
            {
                scale = -1.0f;
            }

            if (scale < SmallestVideoScale)
            {
                scale = SmallestVideoScale;
            }

            unused2 = scale;
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
        target.designator = TokenDesignator(token, target.designator);
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
        switch (token->tag)
        {
        case TagWidth:
            x = token->Float();
            break;
        case TagHeight:
            y = token->Float();
            break;
        case TagDepth:
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
        if (token->tag == TagRumble)
        {
            strength = token->Float();
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
        switch (token->tag)
        {
        case TagDuration:
            ParseTaggedValueRecord(token, &blendTime);
            break;
        case TagJoint:
            joint.id = token->value;
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
        TokenVectorComponent(reader.Current(), &offset.x);
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
        if (token->tag == TagX)
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
        if (token->tag == TagExitPoint)
        {
            hanging.exitPoint = token->value;
        }

        reader.Next();
    }
}

void SetStoredPositionAtAngleCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->tag)
        {
        case TagSize:
            distance = token->Float();
            break;
        case TagAngle:
            angle = token->Float();
            break;
        default:
            break;
        }

        reader.Next();
    }
}

void NowMoveForwardsCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ParseTaggedValueTokens(tokens, &speed);
}

void NoOpNowMoveBackwardsCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ParseTaggedValueTokens(tokens, &unused1);
}

void NowStrafeLeftCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ParseTaggedValueTokens(tokens, &speed);
}

void NowStrafeRightCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ParseTaggedValueTokens(tokens, &speed);
}

void NowTurnLeftCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ParseTaggedValueTokens(tokens, &turnRate);
}

void NowTurnRightCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ParseTaggedValueTokens(tokens, &turnRate);
}

void TriggerLinkedObjectsCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        if (token->tag == TagNone && token->value == KeywordFromOriginator)
        {
            fromOriginator.on = 1;
        }

        reader.Next();
    }
}

void NextLinkedObjectInListCommand::ParseTokens(const ScriptTokenList* tokens)
{
    // A list of the numbers, counted in count. Retail bug: the seventeenth is written over the count (the list goes on from its
    // value) and the bytes can land past the command
    u8* links = numbers;
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        if (token->tag == TagNone)
        {
            links[count.numbers] = static_cast<u8>(token->value);
            count.numbers = count.numbers + 1;
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
        if (token->tag == TagRank)
        {
            number = static_cast<s32>(token->value);
        }

        reader.Next();
    }
}
