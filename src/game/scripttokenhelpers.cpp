#include "game/scripttokens.h"

#include "game/agentlab.h"
#include "game/math.h"
#include "game/objectnode.h"
#include "game/properties.h"

// The helpers the commands' development tools parsers share (game/scripttokens.h): a token's value as a switch, a designator or
// a space, a vector's component, a tagged value, and a motion block's arguments. The retail game never calls them

namespace
{
// The axis a body is slowed along (MotionBlockBody::slowedAxis)
enum SlowedAxis : u32
{
    SlowedAlongX = 1,
    SlowedAlongY = 2,
    SlowedAlongZ = 3,
};

// The designators of the tool's keywords no Designator names: two nothing answers
constexpr u32 UnusedDesignatorF1 = 0xF1;
constexpr u32 UnusedDesignatorF3 = 0xF3;

bool IsKeyword(const ScriptToken* token)
{
    return token->tag == TagNone && token->type == TokenKeyword;
}

void SetSlowedAxis(MotionBlock* block, f32 slowing, u32 axis)
{
    block->slowing = slowing;
    block->body.slowedAxis = axis;
}

// A keyword's setting of a motion block
void ApplyMotionKeyword(MotionBlock* block, u32 keyword)
{
    switch (keyword)
    {
    case KeywordJustMove:
        SetMotionBlockConstraint(block, MotionBlock::GivenHingeX, nullptr);
        SetMotionBlockConstraint(block, MotionBlock::GivenHingeY, nullptr);
        SetMotionBlockConstraint(block, MotionBlock::GivenHingeZ, nullptr);
        break;
    case KeywordCornerSprings:
        SetMotionBlockKind(block, MotionBlock::KindBody);
        break;
    case KeywordFreeSphere:
        SetMotionBlockKind(block, MotionBlock::KindBall);
        break;
    case KeywordFreeCuboid:
        SetMotionBlockKind(block, MotionBlock::KindGrabber);
        break;
    case KeywordResnap:
        block->motion.putsBackStuck = 1;
        break;
    case KeywordUprightLaunched:
        block->flags.uprightsLaunched = 1;
        break;
    case KeywordHoldTouched:
        block->flags.holdsTouched = 1;
        break;
    case KeywordHoldAgentRef1:
        block->flags.holdsAgentRef1 = 1;
        break;
    case KeywordConstraintFixed:
        SetMotionBlockConstraint(block, MotionBlock::ConstraintFixed, nullptr);
        break;
    case KeywordLineX:
        SetMotionBlockConstraint(block, MotionBlock::ConstraintLine, &g_XAxis);
        break;
    case KeywordLineY:
        SetMotionBlockConstraint(block, MotionBlock::ConstraintLine, &g_YAxis);
        break;
    case KeywordLineZ:
        SetMotionBlockConstraint(block, MotionBlock::ConstraintLine, &g_ZAxis);
        break;
    case KeywordPlaneY:
        SetMotionBlockConstraint(block, MotionBlock::ConstraintPlane, &g_YAxis);
        break;
    case KeywordPlaneZ:
        SetMotionBlockConstraint(block, MotionBlock::ConstraintPlane, &g_ZAxis);
        break;
    case KeywordPlaneX:
        SetMotionBlockConstraint(block, MotionBlock::ConstraintPlane, &g_XAxis);
        break;
    case KeywordHingeX:
        SetMotionBlockConstraint(block, MotionBlock::GivenHingeX, nullptr);
        break;
    case KeywordHingeY:
        SetMotionBlockConstraint(block, MotionBlock::GivenHingeY, nullptr);
        break;
    case KeywordHingeZ:
        SetMotionBlockConstraint(block, MotionBlock::GivenHingeZ, nullptr);
        break;
    case KeywordNoCollisions:
        block->body.noCollisions = 1;
        break;
    case KeywordPushedByVolumes:
        block->body.pushedByVolumes = 1;
        break;
    case KeywordNeverPutBack:
        block->body.neverPutBack = 1;
        break;
    default:
        break;
    }
}
}

// The centre of mass is only set once every token is read, when any of its components was given (the others 0)
void ParseMotionBlockTokens(const ScriptTokenList* tokens, MotionBlock* block)
{
    bool centerGiven = false;
    f32 centerX = 0.0f;
    f32 centerY = 0.0f;
    f32 centerZ = 0.0f;
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        f32 value = token->Float();
        switch (token->tag)
        {
        case TagTurn:
            SetMotionBlockTurnLimit(value, block);
            break;
        case TagPower:
        case TagSpringPower:
        case TagStiffness:
            block->springStiffness = value;
            break;
        case TagDamping:
            block->drag = value;
            break;
        case TagGravity:
            block->gravity = value;
            break;
        case TagFriction:
            block->friction = value;
            break;
        case TagBounce:
            block->restitution = value;
            break;
        case TagInertia:
            block->size = value;
            break;
        case TagXShift:
            centerX = value;
            centerGiven = true;
            break;
        case TagYShift:
            centerY = value;
            centerGiven = true;
            break;
        case TagZShift:
            centerZ = value;
            centerGiven = true;
            break;
        case TagAntiRoll:
            block->spinFriction = value;
            break;
        case TagSpringDamping:
            block->springDamping = value;
            break;
        case TagSteady:
            block->lengthDrag = value;
            break;
        case TagFlatMultiplier:
            block->springAcross = value;
            break;
        case TagGrabStrength:
            block->grabStrength = value;
            break;
        case TagHoldStrength:
            block->holdStrength = value;
            break;
        case TagKnockScale:
            block->knockScale = value;
            break;
        case TagSlowingX:
            SetSlowedAxis(block, value, SlowedAlongX);
            break;
        case TagSlowingY:
            SetSlowedAxis(block, value, SlowedAlongY);
            break;
        case TagSlowingZ:
            SetSlowedAxis(block, value, SlowedAlongZ);
            break;
        case TagTurnToFocus:
            block->turnStrength = value;
            block->body.unused10 = 1;
            break;
        case TagSubsteps:
            block->body.substeps = token->value;
            break;
        case TagMass:
            block->mass = value;
            break;
        // On sets the physics body's pushable bits
        case TagPushable:
            block->body.pushable20 = TokenIsOn(token);
            break;
        case TagPushableToo:
            block->body.pushable40 = TokenIsOn(token);
            break;
        case TagBuoyancy:
            block->springStiffness = value;
            block->body.floats = 1;
            break;
        case TagNone:
            if (token->type == TokenKeyword)
            {
                ApplyMotionKeyword(block, token->value);
            }

            break;
        default:
            break;
        }

        reader.Next();
    }

    if (centerGiven)
    {
        block->flags.hasCenterOfMass = 1;
        block->centerOfMass[0] = centerX;
        block->centerOfMass[1] = centerY;
        block->centerOfMass[2] = centerZ;
    }
}

// The tags that name a designator by a keyword (the others' keywords name none); TagAgent's value is one, TagKey's is one more
// than its designator
void ParseDesignatorToken(const ScriptToken* token, u32* designator)
{
    switch (token->tag)
    {
    case TagNone:
    case TagSourceDesignator:
    case TagDestinationDesignator:
    case TagSourceAgent:
    case TagFromDesignator:
    case TagToDesignator:
    case TagFirstTarget:
    case TagSecondTarget:
    case TagFirstTargetPosition:
    case TagSecondTargetPosition:
    case TagAgentDesignator:
        break;
    case TagKey:
        *designator = token->value - 1;
        return;
    case TagAgent:
        *designator = token->value;
        return;
    default:
        return;
    }

    if (token->type != TokenKeyword)
    {
        return;
    }

    switch (token->value)
    {
    case KeywordFocus:
        *designator = DesignatesFocus;
        break;
    case KeywordFocusPosition:
        *designator = DesignatesFocusPosition;
        break;
    case KeywordSelf:
        *designator = DesignatesItself;
        break;
    case KeywordCurrentKey:
        *designator = DesignatesCurrentKey;
        break;
    case KeywordNextKey:
        *designator = DesignatesNextKey;
        break;
    case KeywordCurrentNode:
        *designator = DesignatesCurrentStep;
        break;
    case KeywordNextNode:
        *designator = DesignatesPreviousStep;
        break;
    case KeywordAgentRef1:
        *designator = DesignatesAgentRef1;
        break;
    case KeywordAgentRef2:
        *designator = DesignatesAgentRef2;
        break;
    case KeywordStoredPosition:
        *designator = DesignatesStoredPosition;
        break;
    case KeywordPlayer:
        *designator = DesignatesPlayer;
        break;
    case KeywordOriginator:
        *designator = DesignatesOriginator;
        break;
    case KeywordHeadTarget:
        *designator = DesignatesHeadTarget;
        break;
    case KeywordUnusedDesignator90:
        *designator = UnusedDesignatorF1;
        break;
    case KeywordUnusedDesignator91:
        *designator = UnusedDesignatorF3;
        break;
    case KeywordNextStep:
        *designator = DesignatesNextStep;
        break;
    case KeywordLinkedById:
        *designator = DesignatesLinkedById;
        break;
    default:
        break;
    }
}

// The control packets' spaces the keywords name: world, initial, current, target, parent, initial position and stored
void ParseSpaceToken(const ScriptToken* token, u32* space)
{
    if (!IsKeyword(token))
    {
        return;
    }

    switch (token->value)
    {
    case KeywordWorldSpace:
        *space = ControlPacket::WorldSpace;
        break;
    case KeywordInitialSpace:
        *space = ControlPacket::InitialSpace;
        break;
    case KeywordCurrentSpace:
        *space = ControlPacket::CurrentSpace;
        break;
    case KeywordTargetSpace:
        *space = ControlPacket::TargetSpace;
        break;
    case KeywordTrackedSpace:
        *space = ControlPacket::ParentSpace;
        break;
    case KeywordInitialPosition:
        *space = ControlPacket::InitialPosition;
        break;
    case KeywordStoredSpace:
        *space = ControlPacket::StoredSpace;
        break;
    default:
        break;
    }
}

void ParseTaggedValueTokens(const ScriptTokenList* tokens, TaggedValue* value)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        if (token->tag == TagDegreesPerSecond)
        {
            value->type = TaggedValue::TypeAngle;
            ParseTaggedValueRecord(token, value);
        }
        else if (token->tag == TagMetresPerSecond)
        {
            value->type = TaggedValue::TypeFloat;
            ParseTaggedValueRecord(token, value);
        }

        reader.Next();
    }
}

bool TokenIsOn(const ScriptToken* token)
{
    return token->value == KeywordOn;
}

bool TokenIsOn2(const ScriptToken* token)
{
    return token->value == KeywordOn;
}

u32 TokenSetting(const ScriptToken* token)
{
    return token->value == KeywordOn ? SwitchOn : SwitchOff;
}

u32 TokenDesignator(const ScriptToken* token, u32 designator)
{
    ParseDesignatorToken(token, &designator);
    return designator;
}

u32 TokenSpace(const ScriptToken* token, u32 space)
{
    ParseSpaceToken(token, &space);
    return space;
}

void TokenVectorComponent(const ScriptToken* token, f32* vector)
{
    if (token->tag == TagY)
    {
        vector[1] = token->Float();
    }
    else if (token->tag == TagX)
    {
        vector[0] = token->Float();
    }
    else if (token->tag == TagZ)
    {
        vector[2] = token->Float();
    }
}
