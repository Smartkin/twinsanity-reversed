#include "game/commands.h"

#include "game/collision.h"
#include "game/instanceparticles.h"
#include "game/math.h"
#include "game/objectnode.h"
#include "game/particles.h"
#include "game/progress.h"
#include "game/properties.h"
#include "game/scripttokens.h"
#include "game/sound.h"

// More of the commands' development tools parsers (their vtables' slot 2, game/commandtokens.cpp has the others): each token's
// tag names an argument, its value goes into the command's field. The retail game never calls them

namespace
{
// The axes of the instance's frame SetFocusPosition's position is kept to (SetFocusPositionCommand::Target::keptAxes)
enum KeptAxis : u32
{
    KeepsX = 1 << 0,
    KeepsY = 1 << 1,
    KeepsZ = 1 << 2,
};

// The frame a particle trail's emitter follows (TrailBits::space, game/particletrails.cpp's EmitterFrame): its exit point's,
// where its instance started, its instance's place, the origin
enum TrailSpace : u32
{
    TrailFromExitPoint = 0,
    TrailFromStart = 1,
    TrailFromPlace = 2,
    TrailFromOrigin = 3,
};

// The trails taken away together (TrailBits::removalKind, ParticleTrails::RemoveKind): the ones a runner's end takes away when
// its node keeps its particles, and a kind nothing asks to take away
enum TrailRemoval : u32
{
    RemovedAtRunnerEnd = 1,
    UnusedRemovalKind = 2,
};
}

void UnlinkTargetCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        request.target = TokenDesignator(token, request.target);
        switch (token->tag)
        {
        case TagLinked:
            request.target = token->value - 1;
            request.byIndex = 1;
            break;
        case TagNone:
            if (token->type == TokenKeyword)
            {
                if (token->value == KeywordAll)
                {
                    request.all = 1;
                }
                else if (token->value == KeywordCurrentLinked)
                {
                    request.current = 1;
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
        target.designatorOrIndex = TokenDesignator(token, target.designatorOrIndex);
        switch (token->tag)
        {
        case TagLinked:
            target.designatorOrIndex = token->value - 1;
            target.byIndex = 1;
            break;
        case TagNone:
            if (token->type == TokenKeyword)
            {
                switch (token->value)
                {
                case KeywordHard:
                    target.force = 1;
                    break;
                case KeywordAll:
                    target.everyLinked = 1;
                    break;
                case KeywordCurrentLinked:
                    target.current = 1;
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
        target.designator = TokenDesignator(token, target.designator);
        switch (token->tag)
        {
        case TagBehaviourSlot:
            slot.slot = token->value;
            break;
        case TagActor:
            target.objectId = token->value;
            break;
        case TagRunnerSlot:
            slot.runnerSlot = token->value;
            break;
        case TagNone:
            if (token->type == TokenKeyword)
            {
                if (token->value == KeywordEveryLinked)
                {
                    slot.everyLinked = 1;
                }
                else if (token->value == KeywordNoSource)
                {
                    slot.givesSource = 0;
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
        request.designator = TokenDesignator(token, request.designator);
        if (token->tag == TagRunnerSlot)
        {
            request.unused8 = token->value;
        }
        else if (token->tag == TagNone && token->type == TokenKeyword && token->value == KeywordEveryLinked)
        {
            request.everyLinked = 1;
        }

        reader.Next();
    }
}

void RestoreOwnObjectCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        request.designator = TokenDesignator(token, request.designator);
        if (token->tag == TagNone && token->type == TokenKeyword && token->value == KeywordOwnObject)
        {
            request.own = 1;
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
        switch (token->tag)
        {
        case TagScale:
            ParseTaggedValueRecord(token, &speed);
            flags.speedGiven = 1;
            break;
        case TagDuration:
            ParseTaggedValueRecord(token, &speed);
            flags.speedIsDuration = 1;
            break;
        case TagAnimation:
        {
            // A list of the slots, its count in the flags (past the fourth, past the command)
            u32 count = flags.slotCount;
            u8* animationSlots = slots;
            animationSlots[count] = static_cast<u8>(token->value);
            flags.slotCount = count + 1;
            break;
        }
        case TagBlendSeconds:
            ParseTaggedValueRecord(token, &blendTime);
            flags.blendTimeGiven = 1;
            break;
        case TagJoint:
            flags.joint = token->value;
            break;
        case TagStartPosition:
            flags.startsPartWay = 1;
            startPosition = token->Float();
            break;
        case TagRange:
            ParseTaggedValueRecord(token, &speedRandom);
            flags.speedRandomGiven = 1;
            break;
        case TagShadowSlot:
            flags.shadowGiven = 1;
            flags.shadowSlot = token->value;
            break;
        case TagNone:
            if (token->type == TokenKeyword)
            {
                switch (token->value)
                {
                case KeywordBack:
                    flags.backward = 1;
                    break;
                case KeywordLooped:
                    flags.loops = 1;
                    break;
                case KeywordContinue:
                    flags.continues = 1;
                    break;
                case KeywordAlwaysQueued:
                    flags.queuedAlways = 1;
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
        if (token->tag == TagNone)
        {
            switch (token->value)
            {
            case KeywordUpdateOnce:
                update.mode = AnimationUpdate::UpdateOnce;
                break;
            case KeywordForever:
                update.mode = AnimationUpdate::UpdateAlways;
                break;
            case KeywordNoUpdate:
                update.mode = 0;
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
        target.designator = TokenDesignator(token, target.designator);
        switch (token->tag)
        {
        case TagCounter:
        case TagGameCounter:
            target.counter = token->value;
            break;
        case TagAgentCounter:
        case TagCounterIndex:
            target.counter = token->value;
            target.agentCounter = 1;
            break;
        case TagRandomBelow:
            target.randomBelow = 1;
            value.type = TaggedValue::TypeInt;
            ParseTaggedValueRecord(token, &value);
            break;
        case TagValue:
            value.type = TaggedValue::TypeInt;
            ParseTaggedValueRecord(token, &value);
            break;
        case TagNone:
            if (token->type == TokenInt)
            {
                value.type = TaggedValue::TypeInt;
                ParseTaggedValueRecord(token, &value);
            }
            else if (token->value == KeywordEveryLinked)
            {
                target.everyLinked = 1;
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
        target.designator = TokenDesignator(token, target.designator);
        switch (token->tag)
        {
        case TagValue:
            delta.type = TaggedValue::TypeInt;
            ParseTaggedValueRecord(token, &delta);
            break;
        case TagCounter:
        case TagGameCounter:
            target.counter = token->value;
            break;
        case TagAgentCounter:
        case TagCounterIndex:
            target.counter = token->value;
            target.agentCounter = 1;
            break;
        case TagActor:
            linkedObject.id = token->value;
            break;
        case TagNone:
            if (token->type == TokenInt)
            {
                delta.type = TaggedValue::TypeInt;
                ParseTaggedValueRecord(token, &delta);
            }
            else if (token->value == KeywordEveryLinked)
            {
                target.linkedObjects = 1;
            }

            break;
        default:
            break;
        }

        reader.Next();
    }
}

void AddToFocusCounterCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->tag)
        {
        case TagFocusCounter:
            counter.counter = token->value;
            break;
        case TagNone:
            if (token->type != TokenInt)
            {
                break;
            }

            [[fallthrough]];
        case TagValue:
            amount.type = TaggedValue::TypeInt;
            ParseTaggedValueRecord(token, &amount);
            break;
        default:
            break;
        }

        reader.Next();
    }
}

void SetFocusCounterCommand::ParseTokens(const ScriptTokenList* tokens)
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
            value.type = TaggedValue::TypeInt;
            ParseTaggedValueRecord(token, &value);
            break;
        case TagAgentCounter:
            // Retail has no break here: a source of type 1 is the value as well
            source = static_cast<s32>(token->value);
            [[fallthrough]];
        case TagNone:
            if (token->type == TokenInt)
            {
                value.type = TaggedValue::TypeInt;
                ParseTaggedValueRecord(token, &value);
            }

            break;
        case TagFocusCounter:
            counter.counter = token->value;
            break;
        default:
            break;
        }

        reader.Next();
    }
}

void SetFocusPositionCommand::ParseTokens(const ScriptTokenList* tokens)
{
    // An offset that isn't 0 is given. Retail keeps the target and the options as one 64 bit word of bits
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        target.designator = TokenDesignator(token, target.designator);
        target.space = TokenSpace(token, target.space);
        TokenVectorComponent(token, &offsetX);
        switch (token->tag)
        {
        case TagAlongPerception:
            alongPerception = token->Float();
            break;
        case TagYMultiplier:
            noiseUpScale = token->Float();
            break;
        case TagTolerance:
            target.noise = 1;
            noiseSpread = token->Float();
            break;
        case TagAgent:
            target.receiver = token->value;
            break;
        case TagKeyLocal:
            // The designator's next (none stays none)
            target.unused20 = 1;
            if (target.designator != DesignatesNone)
            {
                target.designator += 1;
            }

            break;
        case TagRouteToward:
        case TagRouteForward:
            routeToward = token->Float();
            break;
        case TagRouteCorner:
            routeCorner = token->Float();
            break;
        case TagRouteScatter:
            routeScatter = token->Float();
            break;
        case TagAwayFromPlayer:
            awayFromPlayer = token->Float();
            break;
        case TagRouteLift:
            routeLift = token->Float();
            break;
        case TagRouteSideways:
            routeSideways = token->Float();
            break;
        case TagAngle:
            options.straightAhead = 1;
            unused50 = token->value;
            break;
        case TagPositionJoint:
            joint.id = token->value;
            break;
        case TagNone:
            if (token->type == TokenKeyword)
            {
                switch (token->value)
                {
                case KeywordKeepX:
                    target.keptAxes |= KeepsX;
                    break;
                case KeywordKeepY:
                    target.keptAxes |= KeepsY;
                    break;
                case KeywordKeepZ:
                    target.keptAxes |= KeepsZ;
                    break;
                case KeywordFromStart:
                    target.fromStart = 1;
                    break;
                case KeywordUnusedCB:
                    target.unused30 = 1;
                    break;
                case KeywordOwnPosition:
                    target.addsMove = 1;
                    break;
                case KeywordOfFocus:
                    options.focusJoint = 1;
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
        target.offsetGiven = 1;
    }
}

void AddNoiseToFocusPositionCommand::ParseTokens(const ScriptTokenList* tokens)
{
    spread.type = TaggedValue::TypeFloat;
    spread.SetFloat(1.0f);
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->tag)
        {
        case TagYMultiplier:
            yScale = token->Float();
            break;
        case TagRange:
            ParseTaggedValueRecord(token, &spread);
            break;
        case TagXMultiplier:
            xScale = token->Float();
            break;
        case TagZMultiplier:
            zScale = token->Float();
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
        if (token->tag == TagAgent)
        {
            target.designator = token->value;
        }
        else if (token->tag == TagNone)
        {
            switch (token->value)
            {
            case KeywordAgentRef2:
                target.designator = DesignatesAgentRef2;
                break;
            case KeywordFocus:
                target.designator = DesignatesFocus;
                break;
            case KeywordAgentRef1:
                target.designator = DesignatesAgentRef1;
                break;
            case KeywordLinkedById:
                target.designator = DesignatesLinkedById;
                break;
            case KeywordParent:
                target.designator = DesignatesParent;
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
        switch (token->tag)
        {
        case TagKeyLocal:
            target.unused8 = 1;
            break;
        case TagKey:
            target.key = token->value - 1;
            break;
        case TagNone:
            switch (token->value)
            {
            case KeywordNextKey:
                target.key = DesignatesNextKey;
                break;
            case KeywordCurrentKey:
                target.key = DesignatesCurrentKey;
                break;
            case KeywordOfFocus:
                target.focusKeys = 1;
                break;
            case KeywordOfAgentRef1:
                target.agentRef1Keys = 1;
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
        target.index = TokenDesignator(token, target.index);
        switch (token->tag)
        {
        case TagLinked:
            // Counted from 1, the first for anything below
            target.index = static_cast<s32>(token->value) > 0 ? token->value - 1 : 0;
            target.linked = 1;
            break;
        case TagNone:
            if (token->type == TokenKeyword)
            {
                switch (token->value)
                {
                case KeywordLast:
                    target.linked = 1;
                    target.last = 1;
                    break;
                case KeywordFirst:
                    target.index = 0;
                    target.linked = 1;
                    break;
                case KeywordCurrentLinked:
                    target.linked = 1;
                    target.current = 1;
                    break;
                case KeywordOfFocus:
                    target.focusLinked = 1;
                    break;
                case KeywordOfAgentRef1:
                    target.agentRef1Linked = 1;
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
        switch (token->tag)
        {
        case TagDestinationDesignator:
            designators.destination = TokenDesignator(token, designators.destination);
            break;
        case TagFromDesignator:
            designators.from = TokenDesignator(token, designators.from);
            break;
        case TagToDesignator:
            designators.to = TokenDesignator(token, designators.to);
            break;
        case TagAlongDistance:
            distance = token->Float();
            break;
        case TagNone:
            if (token->type == TokenKeyword && token->value == KeywordOwnPosition)
            {
                designators.mode = OwnPositionMode;
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
        switch (token->tag)
        {
        case TagX:
            x = token->Float();
            uses.x = 1;
            break;
        case TagY:
            y = token->Float();
            uses.y = 1;
            break;
        case TagZ:
            z = token->Float();
            uses.z = 1;
            break;
        case TagXShift:
            offsetX = token->Float();
            uses.offsetX = 1;
            break;
        case TagYShift:
            offsetY = token->Float();
            uses.offsetY = 1;
            break;
        case TagZShift:
            offsetZ = token->Float();
            uses.offsetZ = 1;
            break;
        case TagSize:
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
    // An offset that isn't 0 is given
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    target.space = ControlPacket::WorldSpace;
    target.receiver = DesignatesNone;
    target.designator = DesignatesNone;
    target.unused20 = 0;
    target.offsetGiven = 0;
    *reinterpret_cast<Vector4*>(&offsetX) = g_DefaultBox.min;
    offsetW = 1.0f;
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        target.designator = TokenDesignator(token, target.designator);
        switch (token->tag)
        {
        case TagKeyLocal:
            // The designator's next (none stays none)
            target.unused20 = 1;
            if (target.designator != DesignatesNone)
            {
                target.designator += 1;
            }

            // Retail has no break here: the token's value goes on into the receiver
            [[fallthrough]];
        case TagAgent:
            target.receiver = token->value;
            break;
        case TagX:
            offsetX = token->Float();
            break;
        case TagY:
            offsetY = token->Float();
            break;
        case TagZ:
            offsetZ = token->Float();
            break;
        case TagKey:
            target.designator = token->value - 1;
            break;
        case TagNone:
            if (token->type == TokenKeyword)
            {
                switch (token->value)
                {
                case KeywordWorldSpace:
                    target.space = ControlPacket::WorldSpace;
                    break;
                case KeywordInitialSpace:
                    target.space = ControlPacket::InitialSpace;
                    break;
                case KeywordCurrentSpace:
                    target.space = ControlPacket::CurrentSpace;
                    break;
                case KeywordTargetSpace:
                    target.space = ControlPacket::TargetSpace;
                    break;
                case KeywordStoredSpace:
                    target.space = ControlPacket::StoredSpace;
                    break;
                case KeywordTilt:
                    target.turnsBody = 1;
                    break;
                case KeywordToContact:
                    target.toContact = 1;
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
        target.offsetGiven = 1;
    }
}

void RotationWarpCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    warp.unused0 = DesignatesNone;
    warp.unused8 = DesignatesNone;
    warp.space = ControlPacket::WorldSpace;
    warp.unused20 = 0;
    warp.rotationGiven = 0;
    warp.levels = 0;
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
        switch (token->tag)
        {
        case TagAgent:
            warp.unused0 = token->value;
            break;
        case TagPitch:
            angleX = token->Float();
            break;
        case TagYaw:
            angleY = token->Float();
            break;
        case TagRoll:
            angleZ = token->Float();
            break;
        case TagKeyLocal:
            warp.unused20 = 1;
            if (warp.unused8 != DesignatesNone)
            {
                warp.unused8 = warp.unused8 + 1;
            }

            break;
        case TagKey:
            warp.unused8 = token->value - 1;
            break;
        case TagNone:
            if (token->type == TokenKeyword)
            {
                switch (token->value)
                {
                case KeywordWorldSpace:
                    warp.space = ControlPacket::WorldSpace;
                    break;
                case KeywordInitialSpace:
                    warp.space = ControlPacket::InitialSpace;
                    break;
                case KeywordCurrentSpace:
                    warp.space = ControlPacket::CurrentSpace;
                    break;
                case KeywordTargetSpace:
                    warp.space = ControlPacket::TargetSpace;
                    break;
                case KeywordStoredSpace:
                    warp.space = ControlPacket::StoredSpace;
                    break;
                case KeywordCurrentKey:
                    warp.unused8 = DesignatesCurrentKey;
                    break;
                case KeywordNextKey:
                    warp.unused8 = DesignatesNextKey;
                    break;
                case KeywordLevel:
                    warp.levels = 1;
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
        warp.rotationGiven = 1;
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

void SnapRotationCommand::ParseTokens(const ScriptTokenList* tokens)
{
    axes.value &= ~AxisSelection::All;
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->tag)
        {
        case TagPitch:
            stepX = token->Float() * DegreesToRadians;
            axes.x = 1;
            break;
        case TagYaw:
            stepY = token->Float() * DegreesToRadians;
            axes.y = 1;
            break;
        case TagRoll:
            stepZ = token->Float() * DegreesToRadians;
            axes.z = 1;
            break;
        default:
            break;
        }

        reader.Next();
    }
}

void WarpAgentCommand::ParseTokens(const ScriptTokenList* tokens)
{
    // An offset that isn't 0 is given
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    warp.space = ControlPacket::WorldSpace;
    warp.receiver = DesignatesNone;
    warp.designator = DesignatesNone;
    warp.unused20 = 0;
    warp.offsetGiven = 0;
    *reinterpret_cast<Vector4*>(&offsetX) = g_DefaultBox.min;
    offsetW = 1.0f;
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        warp.designator = TokenDesignator(token, warp.designator);
        switch (token->tag)
        {
        case TagKey:
            warp.designator = token->value - 1;
            break;
        case TagX:
            offsetX = token->Float();
            break;
        case TagY:
            offsetY = token->Float();
            break;
        case TagZ:
            offsetZ = token->Float();
            break;
        case TagAgent:
            warp.receiver = token->value;
            break;
        case TagToDesignator:
            source.designator = TokenDesignator(token, source.designator);
            break;
        case TagKeyLocal:
            // The designator's next (none stays none)
            warp.unused20 = 1;
            if (warp.designator != DesignatesNone)
            {
                warp.designator += 1;
            }

            // Retail has no break here: the token also names the agent warped
            [[fallthrough]];
        case TagAgentDesignator:
            warp.agent = TokenDesignator(token, warp.agent);
            break;
        case TagNone:
            if (token->type == TokenKeyword)
            {
                switch (token->value)
                {
                case KeywordWorldSpace:
                    warp.space = ControlPacket::WorldSpace;
                    break;
                case KeywordInitialSpace:
                    warp.space = ControlPacket::InitialSpace;
                    break;
                case KeywordCurrentSpace:
                    warp.space = ControlPacket::CurrentSpace;
                    break;
                case KeywordTargetSpace:
                    warp.space = ControlPacket::TargetSpace;
                    break;
                case KeywordStoredSpace:
                    warp.space = ControlPacket::StoredSpace;
                    break;
                case KeywordTilt:
                    warp.turnsBody = 1;
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
        warp.offsetGiven = 1;
    }
}

void RotateAgentCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    warp.unused0 = DesignatesNone;
    warp.unused8 = DesignatesNone;
    warp.space = ControlPacket::WorldSpace;
    warp.unused20 = 0;
    warp.rotationGiven = 0;
    warp.levels = 0;
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
        switch (token->tag)
        {
        case TagKey:
            warp.unused8 = token->value - 1;
            break;
        case TagPitch:
            angleX = token->Float();
            break;
        case TagYaw:
            angleY = token->Float();
            break;
        case TagRoll:
            angleZ = token->Float();
            break;
        case TagAgent:
            warp.unused0 = token->value;
            break;
        case TagToDesignator:
            agent.unused8 = TokenDesignator(token, agent.unused8);
            break;
        case TagKeyLocal:
            warp.unused20 = 1;
            if (warp.unused8 != DesignatesNone)
            {
                warp.unused8 = warp.unused8 + 1;
            }

            break;
        case TagAgentDesignator:
            agent.designator = TokenDesignator(token, agent.designator);
            break;
        case TagNone:
            if (token->type == TokenKeyword)
            {
                switch (token->value)
                {
                case KeywordWorldSpace:
                    warp.space = ControlPacket::WorldSpace;
                    break;
                case KeywordInitialSpace:
                    warp.space = ControlPacket::InitialSpace;
                    break;
                case KeywordCurrentSpace:
                    warp.space = ControlPacket::CurrentSpace;
                    break;
                case KeywordTargetSpace:
                    warp.space = ControlPacket::TargetSpace;
                    break;
                case KeywordStoredSpace:
                    warp.space = ControlPacket::StoredSpace;
                    break;
                case KeywordCurrentKey:
                    warp.unused8 = DesignatesCurrentKey;
                    break;
                case KeywordNextKey:
                    warp.unused8 = DesignatesNextKey;
                    break;
                case KeywordLevel:
                    warp.levels = 1;
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
        warp.rotationGiven = 1;
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
        switch (token->tag)
        {
        case TagMetresPerSecond:
            speed = token->Float();
            break;
        case TagXMagnitude:
            maxDistance = token->Float();
            break;
        case TagNone:
            if (token->type == TokenKeyword)
            {
                switch (token->value)
                {
                case KeywordFocus:
                    target.designator = DesignatesFocus;
                    break;
                case KeywordAgentRef1:
                    target.designator = DesignatesAgentRef1;
                    break;
                case KeywordAgentRef2:
                    target.designator = DesignatesAgentRef2;
                    break;
                case KeywordFocusPosition:
                    target.designator = DesignatesFocusPosition;
                    break;
                case KeywordInitialSpace:
                    target.space = ControlPacket::InitialSpace;
                    break;
                case KeywordCurrentSpace:
                    target.space = ControlPacket::CurrentSpace;
                    break;
                case KeywordOrient:
                    target.faces = 1;
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
        switch (token->tag)
        {
        case TagMetresPerSecond:
            unused2 = token->Float();
            break;
        case TagMoveSpeed:
            speed = token->Float();
            break;
        case TagUnusedEB:
            unused4 = token->Float();
            break;
        case TagSpeedLimit:
            unused5 = token->Float();
            break;
        case TagNone:
            if (token->type == TokenKeyword)
            {
                switch (token->value)
                {
                case KeywordFocus:
                    target.designator = DesignatesFocus;
                    break;
                case KeywordFocusPosition:
                    target.designator = DesignatesFocusPosition;
                    break;
                case KeywordAgentRef1:
                    target.designator = DesignatesAgentRef1;
                    break;
                case KeywordAgentRef2:
                    target.designator = DesignatesAgentRef2;
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

void AddToLinkedCounterCommand::ParseTokens(const ScriptTokenList* tokens)
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
            amount.type = TaggedValue::TypeInt;
            ParseTaggedValueRecord(token, &amount);
            break;
        case TagActor:
            counter.object = token->value;
            break;
        case TagCounterIndex:
            counter.index = token->value;
            break;
        case TagLinked:
            linked.index = token->value - 1;
            break;
        case TagNone:
            if (token->type == TokenKeyword)
            {
                switch (token->value)
                {
                case KeywordAll:
                    linked.every = 1;
                    break;
                case KeywordCurrentLinked:
                    linked.current = 1;
                    break;
                case KeywordFirst:
                    linked.first = 1;
                    break;
                case KeywordLast:
                    linked.last = 1;
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

void PickLinkedObjectNearPlayerCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->tag)
        {
        case TagYaw:
            pick.turned = 1;
            degrees = token->Float();
            break;
        case TagLeadSeconds:
            pick.leads = 1;
            leadSeconds = token->Float();
            break;
        case TagRangeEnd:
            pick.unused12 = token->value;
            break;
        case TagRangeFirst:
            pick.unused4 = token->value;
            break;
        case TagNone:
            if (token->type == TokenKeyword)
            {
                if (token->value == KeywordIdle)
                {
                    pick.passesBusy = 1;
                }
                else if (token->value == KeywordMakeBusy)
                {
                    pick.marksBusy = 1;
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
        switch (token->tag)
        {
        case TagKey:
            key.index = token->value - 1;
            break;
        case TagMinKey:
            key.rangeStart = token->value - 1;
            break;
        case TagMaxKey:
            key.rangeEnd = token->value - 1;
            break;
        case TagKeyLocal:
            key.unused24 = 1;
            break;
        case TagNone:
            if (token->type == TokenKeyword)
            {
                switch (token->value)
                {
                case KeywordNoRepeat:
                    key.noRepeat = 1;
                    break;
                case KeywordNearest:
                    key.nearest = 1;
                    break;
                case KeywordInOrder:
                    key.random = 0;
                    break;
                case KeywordLast:
                    key.last = 1;
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
    u32 exitPoint = NoParticleExitPoint;
    u32 system = NoParticleSystem;
    u32 emitterValue = 1;
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    f32 x = 0.0f;
    f32 y = 0.0f;
    f32 z = 0.0f;
    placement.turned = 0;
    placement.hasPosition = 0;
    placement.axes = 0;
    placement.designator = DesignatesNone;
    placement.surfaceMode = ParticlePlacement::NoSurfaceMode;
    placement.key = Waypoints::NoKey;
    emission.fromFrame = 1;
    emission.fromExitPoint = 0;
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->tag)
        {
        case TagX:
            x = token->Float();
            break;
        case TagY:
            y = token->Float();
            break;
        case TagZ:
            z = token->Float();
            break;
        case TagKey:
            placement.key = token->value - 1;
            break;
        case TagCount:
            emitterValue = token->value;
            break;
        case TagExitPoint:
            exitPoint = token->value;
            break;
        case TagParticle:
            system = token->value;
            break;
        case TagFromDesignator:
            placement.designator = TokenDesignator(token, placement.designator);
            placement.hasPosition = 1;
            emission.fromFrame = 0;
            break;
        case TagNone:
            if (token->type == TokenKeyword)
            {
                switch (token->value)
                {
                case KeywordUp:
                    placement.axes = AxesAsAre;
                    break;
                case KeywordDown:
                    placement.axes = AxesYNegated;
                    break;
                case KeywordForward:
                    placement.axes = AxesYZSwapped;
                    break;
                case KeywordBack:
                    placement.axes = AxesXNegatedYZSwapped;
                    break;
                case KeywordLeft:
                    placement.axes = AxesXYSwappedNegated;
                    break;
                case KeywordRight:
                    placement.axes = AxesXYSwapped;
                    break;
                case KeywordGlobal:
                    emission.fromFrame = 0;
                    break;
                case KeywordOrient:
                    placement.turned = 1;
                    break;
                case KeywordOnImpact:
                    placement.surfaceMode = ContactImpact;
                    break;
                case KeywordOnStep:
                    placement.surfaceMode = ContactStep1;
                    break;
                case KeywordOnOtherStep:
                    placement.surfaceMode = ContactStep2;
                    break;
                case KeywordOnLand:
                    placement.surfaceMode = ContactLand;
                    break;
                case KeywordOnHardImpact:
                    placement.surfaceMode = ContactHardImpact;
                    break;
                case KeywordOnScrape:
                    placement.surfaceMode = ContactScrape;
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

    emission.system = system;
    emission.emitterValue = emitterValue;
    if (x != 0.0f || y != 0.0f || z != 0.0f)
    {
        position.x = x;
        position.y = y;
        position.z = z;
        position.w = 1.0f;
        placement.hasPosition = 1;
    }

    if (exitPoint < NoParticleExitPoint)
    {
        emission.fromExitPoint = 1;
        emission.exitPoint = exitPoint;
    }
    else
    {
        emission.fromExitPoint = 0;
        emission.exitPoint = NoParticleExitPoint;
    }
}

void NoOp197Command::ParseTokens(const ScriptTokenList* tokens)
{
    // A position that isn't 0 goes into unused6 to unused9 (unused9 the w) and sets hasOffset
    f32 x = 0.0f;
    f32 y = 0.0f;
    f32 z = 0.0f;
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->tag)
        {
        case TagXShift:
            x = token->Float();
            break;
        case TagYShift:
            y = token->Float();
            break;
        case TagZShift:
            z = token->Float();
            break;
        case TagDuration:
            unused2 = token->Float();
            break;
        case TagExitPoint:
            unused1.hasExitPoint = 1;
            unused1.exitPoint = token->value;
            break;
        case TagChildActor:
            unused1.object = token->value;
            break;
        default:
            break;
        }

        reader.Next();
    }

    if (x != 0.0f || y != 0.0f || z != 0.0f)
    {
        unused6 = x;
        unused7 = y;
        unused8 = z;
        unused9 = 1.0f;
        unused1.hasOffset = 1;
    }
}

void AddTrailCommand::ParseTokens(const ScriptTokenList* tokens)
{
    f32 x = 0.0f;
    f32 y = 0.0f;
    f32 z = 0.0f;
    u32 exitPoint = NoParticleExitPoint;
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->tag)
        {
        case TagX:
            x = token->Float();
            break;
        case TagY:
            y = token->Float();
            break;
        case TagZ:
            z = token->Float();
            break;
        case TagVolume:
            trail.bits.volumeGiven = 1;
            trail.volume = token->Float();
            break;
        case TagExitPoint:
            exitPoint = token->value;
            break;
        case TagParticle:
            trail.system = static_cast<u16>(token->value);
            break;
        case TagSound:
        {
            // A list of sound slots, its count in the bits (past the eighth, into the halfwords after them)
            u32 count = trail.bits.soundCount;
            trail.bits.soundCount = count + 1;
            u16* sounds = trail.sounds;
            sounds[count] = static_cast<u16>(token->value);
            break;
        }
        case TagSoundBank:
            switch (token->value)
            {
            case KeywordFx1:
                trail.bits.soundGroup = EffectsGroup;
                break;
            case KeywordFx2:
                trail.bits.soundGroup = SecondEffectsGroup;
                break;
            case KeywordMusic:
                trail.bits.soundGroup = MusicGroup;
                break;
            case KeywordSpeech:
                trail.bits.soundGroup = MovieGroup;
                break;
            default:
                break;
            }

            // Retail has no break here: the value goes on into the halfword after the sounds
            [[fallthrough]];
        case TagDecal:
            trail.extraSound = static_cast<u16>(token->value);
            break;
        case TagAcceleration:
        {
            f32 acceleration = token->Float();
            trail.bits.kind = TrailArguments::KindChanging;
            trail.threshold = acceleration * __builtin_fabsf(acceleration);
            break;
        }
        case TagVelocity:
        {
            f32 speed = token->Float();
            trail.bits.kind = TrailArguments::KindFaster;
            trail.threshold = speed * __builtin_fabsf(speed);
            break;
        }
        case TagInterval:
            trail.bits.kind = TrailArguments::KindTimed;
            trail.interval = token->Float();
            break;
        case TagScalar:
            trail.bits.bySpacing = 1;
            trail.spacing = token->Float();
            break;
        case TagTurn:
        {
            f32 turn = token->Float();
            trail.bits.kind = TrailArguments::KindUnsetVector;
            trail.threshold = turn * __builtin_fabsf(turn);
            break;
        }
        case TagMessage:
            trail.message = static_cast<u16>(token->value);
            break;
        case TagRandomPitch:
            trail.bits.randomPitch = 1;
            trail.randomPitch = token->Float();
            break;
        case TagRelativePitch:
            trail.bits.pitchGiven = 1;
            trail.pitch = token->Float();
            break;
        case TagPitchScalar:
            trail.bits.pitchByThreshold = 1;
            trail.threshold = token->Float();
            break;
        case TagShakeX:
            trail.bits.shakesCamera = 1;
            trail.shakeX = token->Float();
            break;
        case TagShakeY:
            trail.bits.shakesCamera = 1;
            trail.shakeY = token->Float();
            break;
        case TagShakeStrength:
            trail.bits.shakesCamera = 1;
            trail.shakeStrength = token->Float();
            break;
        case TagShakeFalloff:
            trail.bits.shakesCamera = 1;
            trail.shakeFalloff = token->Float();
            break;
        case TagTrailProgress:
            trail.bits.kind = TrailArguments::KindAnimation;
            trail.bits.unused33 = 1;
            trail.progress = token->Float();
            break;
        case TagTrailStrength:
            trail.bits.kind = TrailArguments::KindStronger;
            trail.strength = token->Float();
            break;
        case TagRandomInterval:
            trail.bits.kind = TrailArguments::KindTimed;
            trail.randomInterval = token->Float();
            break;
        case TagDecals:
            trail.bits.decalCount = token->value;
            break;
        case TagNone:
            if (token->type == TokenKeyword)
            {
                switch (token->value)
                {
                case KeywordInitialSpace:
                    trail.bits.space = TrailFromStart;
                    break;
                case KeywordCurrentSpace:
                    trail.bits.space = TrailFromPlace;
                    break;
                case KeywordTargetSpace:
                    trail.bits.space = TrailFromOrigin;
                    break;
                case KeywordAlways:
                    trail.bits.kind = TrailArguments::KindAlways;
                    break;
                case KeywordOrient:
                    trail.bits.turned = 1;
                    break;
                case KeywordUp:
                    trail.bits.axes = AxesAsAre;
                    break;
                case KeywordDown:
                    trail.bits.axes = AxesYNegated;
                    break;
                case KeywordForward:
                    trail.bits.axes = AxesYZSwapped;
                    break;
                case KeywordBack:
                    trail.bits.axes = AxesXNegatedYZSwapped;
                    break;
                case KeywordLeft:
                    trail.bits.axes = AxesXYSwappedNegated;
                    break;
                case KeywordRight:
                    trail.bits.axes = AxesXYSwapped;
                    break;
                case KeywordOnImpact:
                    trail.bits.contact = ContactImpact;
                    break;
                case KeywordOnStep:
                    trail.bits.contact = ContactStep1;
                    break;
                case KeywordOnOtherStep:
                    trail.bits.contact = ContactStep2;
                    break;
                case KeywordOnLand:
                    trail.bits.contact = ContactLand;
                    break;
                case KeywordOnHardImpact:
                    trail.bits.contact = ContactHardImpact;
                    break;
                case KeywordOnScrape:
                    trail.bits.contact = ContactScrape;
                    break;
                case KeywordEndsWithRunner:
                    trail.bits.removalKind = RemovedAtRunnerEnd;
                    break;
                case KeywordUnusedEA:
                    trail.bits.removalKind = UnusedRemovalKind;
                    break;
                case KeywordGravityFrame:
                    trail.bits.gravityFrame = 1;
                    break;
                case KeywordNoSurfaceSound:
                    trail.bits.noSurfaceSound = 1;
                    break;
                default:
                    // Any other keyword clears the space the space keywords set
                    trail.bits.space = TrailFromExitPoint;
                    break;
                }
            }

            break;
        default:
            break;
        }

        reader.Next();
    }

    // Kind 1's interval goes from the interval less the random extra to it plus the extra
    if (trail.bits.kind == TrailArguments::KindTimed)
    {
        f32 extra = trail.randomInterval;
        trail.randomInterval = extra + extra;
        trail.interval = trail.interval + extra;
    }

    if (x != 0.0f || y != 0.0f || z != 0.0f)
    {
        Vector4 position = {x, y, z, 1.0f};
        SetTrailPosition(reinterpret_cast<u32*>(&trail), &position);
    }

    if (exitPoint != NoParticleExitPoint)
    {
        trail.exitPoint = static_cast<u8>(exitPoint);
    }
}
