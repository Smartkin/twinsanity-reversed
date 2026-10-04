#include "game/scripttokens.h"

#include "game/math.h"
#include "game/objectnode.h"
#include "game/properties.h"

// The helpers the commands' development tools parsers share (game/scripttokens.h): a token's value as a setting, a designator or
// a space, a vector's component, a tagged value, and a motion block's arguments. The retail game never calls them

extern "C"
{
}


namespace
{
constexpr u16 KeywordKind = 0xFFFF;
constexpr u8 KeywordType = 4;

// SetBodyConstraint's constraints
enum Constraint : u32
{
    ConstraintFixed = 1,
    ConstraintLine = 2,
    ConstraintPlane = 3,
    HingeX = 4,
    HingeY = 5,
    HingeZ = 6,
};

// The designators of the AgentLab tool's keywords no Designator names: 0xF1, 0xF3, the originator and the route step
constexpr u32 Designates0xF1 = 0xF1;
constexpr u32 Designates0xF3 = 0xF3;
constexpr u32 DesignatesOriginator = 0xF4;
constexpr u32 DesignatesRouteStep = 0xEF;

// The motion blocks' bits no MotionBlock enum names
constexpr u32 FlagUnknown400 = 0x400;
constexpr u32 FlagUnknown800 = 0x800;
constexpr u32 FlagUnknown1000 = 0x1000;
constexpr u32 BodyTurnsToFocus = 0x400;
constexpr u32 BodyUnknown4000 = 0x4000;

bool IsKeyword(const ScriptToken* token)
{
    return token->kind == KeywordKind && token->type == KeywordType;
}

void SetSlowedAxis(MotionBlock* block, f32 slowing, u32 axis)
{
    constexpr u32 SlowedBits = MotionBlock::SlowedMask << MotionBlock::SlowedShift;
    block->slowing = slowing;
    block->bodyBits = (block->bodyBits & ~SlowedBits) | axis << MotionBlock::SlowedShift;
}

void SetBodyBit(MotionBlock* block, u32 bit, bool set)
{
    if (set)
    {
        block->bodyBits |= bit;
    }
    else
    {
        block->bodyBits &= ~bit;
    }
}

// A keyword's setting of a motion block
void ApplyMotionKeyword(MotionBlock* block, u32 keyword)
{
    switch (keyword)
    {
    case 0x50:
        SetMotionBlockConstraint(block, HingeX, nullptr);
        SetMotionBlockConstraint(block, HingeY, nullptr);
        SetMotionBlockConstraint(block, HingeZ, nullptr);
        break;
    case 0x5A:
        SetMotionBlockKind(block, MotionBlock::KindBody);
        break;
    case 0x5B:
        SetMotionBlockKind(block, MotionBlock::KindBall);
        break;
    case 0x5C:
        SetMotionBlockKind(block, MotionBlock::KindGrabber);
        break;
    case 0x5D:
        block->cycles |= MotionBlock::PutsBackStuck;
        break;
    case 0x7F:
        block->flags |= FlagUnknown400;
        break;
    case 0x80:
        block->flags |= FlagUnknown800;
        break;
    case 0x81:
        block->flags |= FlagUnknown1000;
        break;
    case 0x92:
        SetMotionBlockConstraint(block, ConstraintFixed, nullptr);
        break;
    case 0x93:
        SetMotionBlockConstraint(block, ConstraintLine, &g_XAxis);
        break;
    case 0x94:
        SetMotionBlockConstraint(block, ConstraintLine, &g_YAxis);
        break;
    case 0x95:
        SetMotionBlockConstraint(block, ConstraintLine, &g_ZAxis);
        break;
    case 0x96:
        SetMotionBlockConstraint(block, ConstraintPlane, &g_YAxis);
        break;
    case 0x97:
        SetMotionBlockConstraint(block, ConstraintPlane, &g_ZAxis);
        break;
    case 0x98:
        SetMotionBlockConstraint(block, ConstraintPlane, &g_XAxis);
        break;
    case 0x99:
        SetMotionBlockConstraint(block, HingeX, nullptr);
        break;
    case 0x9A:
        SetMotionBlockConstraint(block, HingeY, nullptr);
        break;
    case 0x9B:
        SetMotionBlockConstraint(block, HingeZ, nullptr);
        break;
    case 0xA2:
        block->bodyBits |= MotionBlock::NoCollisions;
        break;
    case 0xD1:
        block->bodyBits |= BodyUnknown4000;
        break;
    case 0x10B:
        block->bodyBits |= MotionBlock::NeverPutBack;
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
        switch (token->kind)
        {
        case 0x1C:
            SetMotionBlockTurnLimit(value, block);
            break;
        case 0x3E:
        case 0x8C:
        case 0xA5:
            block->springStiffness = value;
            break;
        case 0x3F:
            block->drag = value;
            break;
        case 0x6A:
            block->gravity = value;
            break;
        case 0x73:
            block->friction = value;
            break;
        case 0x74:
            block->restitution = value;
            break;
        case 0x79:
            block->size = value;
            break;
        case 0x82:
            centerX = value;
            centerGiven = true;
            break;
        case 0x83:
            centerY = value;
            centerGiven = true;
            break;
        case 0x84:
            centerZ = value;
            centerGiven = true;
            break;
        case 0x8B:
            block->spinFriction = value;
            break;
        case 0x8D:
            block->springDamping = value;
            break;
        case 0x8F:
            block->lengthDrag = value;
            break;
        case 0x90:
            block->springAcross = value;
            break;
        case 0xA1:
            block->grabStrength = value;
            break;
        case 0xA4:
            block->holdStrength = value;
            break;
        case 0xCE:
            block->bodyUnknown44 = value;
            break;
        case 0xD0:
            SetSlowedAxis(block, value, 1);
            break;
        case 0xD1:
            SetSlowedAxis(block, value, 2);
            break;
        case 0xD2:
            SetSlowedAxis(block, value, 3);
            break;
        case 0xF3:
            block->turnStrength = value;
            block->bodyBits |= BodyTurnsToFocus;
            break;
        case 0xF9:
            block->bodyBits = (block->bodyBits & ~(MotionBlock::SubstepsMask << MotionBlock::SubstepsShift)) |
                              (token->value & MotionBlock::SubstepsMask) << MotionBlock::SubstepsShift;
            break;
        case 0xFD:
            block->mass = value;
            break;
        // A 0 sets the physics body's flags
        case 0x120:
            SetBodyBit(block, MotionBlock::PhysicsFlag20, TokenIsZero(token));
            break;
        case 0x121:
            SetBodyBit(block, MotionBlock::PhysicsFlag40, TokenIsZero(token));
            break;
        case 0x12C:
            block->springStiffness = value;
            block->bodyBits |= MotionBlock::Floats;
            break;
        case KeywordKind:
            if (token->type == KeywordType)
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
        block->flags |= MotionBlock::HasCenterOfMass;
        block->centerOfMass[0] = centerX;
        block->centerOfMass[1] = centerY;
        block->centerOfMass[2] = centerZ;
    }
}

// The kinds that name a designator by a keyword (the others' keywords name none); kind 6's value is one, kind 7's is one more
// than its designator
void ParseDesignatorToken(const ScriptToken* token, u32* designator)
{
    switch (token->kind)
    {
    case KeywordKind:
    case 0xC7:
    case 0xC8:
    case 0xEC:
    case 0x104:
    case 0x105:
    case 0x107:
    case 0x108:
    case 0x110:
    case 0x111:
    case 0x117:
        break;
    case 0x7:
        *designator = token->value - 1;
        return;
    case 0x6:
        *designator = token->value;
        return;
    default:
        return;
    }

    if (token->type != KeywordType)
    {
        return;
    }

    switch (token->value)
    {
    case 0x1C:
        *designator = DesignatesFocus;
        break;
    case 0x1D:
        *designator = DesignatesFocusPosition;
        break;
    case 0x20:
        *designator = DesignatesItself;
        break;
    case 0x24:
        *designator = DesignatesCurrentKey;
        break;
    case 0x25:
        *designator = DesignatesNextKey;
        break;
    case 0x2C:
        *designator = DesignatesCurrentStep;
        break;
    case 0x2D:
        *designator = DesignatesPreviousStep;
        break;
    case 0x74:
        *designator = DesignatesAgentRef1;
        break;
    case 0x75:
        *designator = DesignatesAgentRef2;
        break;
    case 0x7A:
        *designator = DesignatesStoredPosition;
        break;
    case 0x88:
        *designator = DesignatesPlayer;
        break;
    case 0x89:
        *designator = DesignatesOriginator;
        break;
    case 0x8F:
        *designator = DesignatesHeadTarget;
        break;
    case 0x90:
        *designator = Designates0xF1;
        break;
    case 0x91:
        *designator = Designates0xF3;
        break;
    case 0xB1:
        *designator = DesignatesRouteStep;
        break;
    case 0xCE:
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
    case 0x2:
        *space = 0;
        break;
    case 0x3:
        *space = 1;
        break;
    case 0x4:
        *space = 2;
        break;
    case 0x5:
        *space = 3;
        break;
    case 0x7C:
        *space = 4;
        break;
    case 0xA1:
        *space = 5;
        break;
    case 0x21:
        *space = 7;
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
        if (token->kind == 0x38)
        {
            value->raw = (value->raw & ~TaggedValue::TypeMask) | TaggedValue::TypeAngle << TaggedValue::TypeShift;
            ParseTaggedValueRecord(token, value);
        }
        else if (token->kind == 0x39)
        {
            value->raw = (value->raw & ~TaggedValue::TypeMask) | TaggedValue::TypeFloat << TaggedValue::TypeShift;
            ParseTaggedValueRecord(token, value);
        }

        reader.Next();
    }
}

bool TokenIsZero(const ScriptToken* token)
{
    return token->value == 0;
}

bool TokenIsZero2(const ScriptToken* token)
{
    return token->value == 0;
}

u32 TokenSetting(const ScriptToken* token)
{
    return token->value == 0 ? 1 : 2;
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
    if (token->kind == 1)
    {
        vector[1] = token->Float();
    }
    else if (token->kind == 0)
    {
        vector[0] = token->Float();
    }
    else if (token->kind == 2)
    {
        vector[2] = token->Float();
    }
}
