#include "game/commands.h"

#include "game/attachment.h"
#include "game/math.h"
#include "game/objectnode.h"
#include "game/progress.h"
#include "game/scripttokens.h"

// More of the commands' development tools parsers (game/commandtokens.cpp): each token's tag names an argument, its value goes
// into the command's field. The retail game never calls them

namespace
{
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
        if (token->tag == TagState)
        {
            state.flag = TokenIsOn(token);
        }

        reader.Next();
    }
}

void SetPathIndexCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        if (token->tag == TagNone && token->type == TokenInt)
        {
            path.index = token->value;
        }

        reader.Next();
    }
}

void SetPresenceCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        if (token->tag == TagPerceptionWeight)
        {
            presence = token->Float();
        }

        reader.Next();
    }
}

void AddPresenceCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        if (token->tag == TagPerceptionWeight)
        {
            delta = token->Float();
        }

        reader.Next();
    }
}

void SetShadowSlotCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        if (token->tag == TagShadowSlot)
        {
            slot.slot = token->value;
        }

        reader.Next();
    }
}

void ClearShadowSlotCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        if (token->tag == TagShadowSlot)
        {
            slot.slot = token->value;
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
        if (token->tag == TagBlendSeconds)
        {
            fadeSeconds = token->Float();
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
        switch (token->tag)
        {
        case TagVolume:
        {
            // A volume of at most 1
            f32 loudness = token->Float();
            if (1.0f < loudness)
            {
                loudness = 1.0f;
            }

            volume = loudness;
            sets.volume = 1;
            break;
        }
        case TagRelativePitch:
            pitch = token->Float();
            sets.pitch = 1;
            break;
        default:
            break;
        }

        reader.Next();
    }
}

void SetCollisionsCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->tag)
        {
        case TagXShift:
            settings.centerOfMassGiven = 1;
            centerOfMassX = token->Float();
            break;
        case TagYShift:
            settings.centerOfMassGiven = 1;
            centerOfMassY = token->Float();
            break;
        case TagZShift:
            settings.centerOfMassGiven = 1;
            centerOfMassZ = token->Float();
            break;
        case TagGravity:
            ParseTaggedValueRecord(token, &gravity);
            settings.gravityGiven = 1;
            break;
        case TagAgentPairs:
            // The keywords pick the kind of motion, whatever the token's type
            switch (token->value)
            {
            case KeywordOff:
                settings.motionKindGiven = 1;
                settings.motionKind = BodyKindNone;
                break;
            case KeywordPush:
                settings.motionKindGiven = 1;
                settings.motionKind = BodyKindPlain;
                break;
            case KeywordSlide:
                settings.motionKindGiven = 1;
                settings.motionKind = BodyKindSlide;
                break;
            case KeywordSphere:
                settings.motionKindGiven = 1;
                settings.motionKind = BodyKindSphere;
                break;
            case KeywordUpright:
                settings.motionKindGiven = 1;
                settings.motionKind = BodyKindUpright;
                break;
            case KeywordCuboid:
                settings.motionKindGiven = 1;
                settings.motionKind = BodyKindCuboid;
                break;
            case KeywordSimplex:
                settings.motionKindGiven = 1;
                settings.motionKind = BodyKindSimplex;
                break;
            case KeywordAxisBox:
                settings.motionKindGiven = 1;
                settings.motionKind = BodyKindAxisBox;
                break;
            default:
                break;
            }

            break;
        case TagTerrain:
            // The same of the kind of collisions, without the axis box
            switch (token->value)
            {
            case KeywordOff:
                settings.collisionKindGiven = 1;
                settings.collisionKind = BodyKindNone;
                break;
            case KeywordPush:
                settings.collisionKindGiven = 1;
                settings.collisionKind = BodyKindPlain;
                break;
            case KeywordSlide:
                settings.collisionKindGiven = 1;
                settings.collisionKind = BodyKindSlide;
                break;
            case KeywordSphere:
                settings.collisionKindGiven = 1;
                settings.collisionKind = BodyKindSphere;
                break;
            case KeywordUpright:
                settings.collisionKindGiven = 1;
                settings.collisionKind = BodyKindUpright;
                break;
            case KeywordCuboid:
                settings.collisionKindGiven = 1;
                settings.collisionKind = BodyKindCuboid;
                break;
            case KeywordSimplex:
                settings.collisionKindGiven = 1;
                settings.collisionKind = BodyKindSimplex;
                break;
            default:
                break;
            }

            break;
        case TagDamping:
            settings.dragGiven = 1;
            drag = token->Float();
            break;
        case TagLogicalRadius:
            rollRadius = token->Float();
            settings.rollRadiusGiven = 1;
            break;
        case TagInertia:
            settings.sizeGiven = 1;
            size = token->Float();
            break;
        case TagFriction:
        case TagRepelForce:
            settings.frictionGiven = 1;
            friction = token->Float();
            break;
        case TagBounce:
            settings.restitutionGiven = 1;
            restitution = token->Float();
            break;
        case TagTiltPower:
            settings.ownNormalRate = 1;
            restitution = token->Float();
            break;
        case TagMaxTilt:
        {
            s32 angle;
            AngleFrom(&angle, token->Float(), AngleDegrees);
            size = CosOfAngle(&angle);
            settings.slopeLimited = 1;
            break;
        }
        case TagAntiRoll:
            settings.spinFrictionGiven = 1;
            spinFriction = token->Float();
            break;
        case TagSteady:
            settings.lengthDragGiven = 1;
            lengthDrag = token->Float();
            break;
        case TagFlatMultiplier:
            widthScale = token->Float();
            break;
        case TagKnockScale:
            impulseLength = token->Float();
            break;
        case TagImpulseCap:
            settings.impulseCapped = 1;
            impulseLength = token->Float();
            break;
        case TagFixedImpulse:
            settings.impulseFixed = 1;
            impulseLength = token->Float();
            break;
        case TagNone:
            if (token->type == TokenKeyword)
            {
                switch (token->value)
                {
                case KeywordInfiniteMass:
                    settings.immovable = 1;
                    break;
                case KeywordJustVertical:
                    settings.launch = VerticalLaunch;
                    break;
                case KeywordFullMotion:
                    settings.launch = ObjectRigidBodyBits::Launched;
                    break;
                case KeywordNoPhysicsMotion:
                    settings.launch = NoLaunch;
                    break;
                case KeywordClampNormal:
                    settings.steersItself = 1;
                    break;
                case KeywordUnused8E:
                    settings.unused25 = 1;
                    break;
                case KeywordOrient:
                    settings.unused26 = 1;
                    break;
                case KeywordEnclosingSphere:
                    settings.rollRadiusFromBox = 1;
                    break;
                case KeywordRadiusFromHeight:
                    settings.rollRadiusFromHeight = 1;
                    break;
                case KeywordPushedByVolumes:
                    // Falls through into the default (no break in retail)
                    settings.pushedByVolumes = 1;
                    [[fallthrough]];
                default:
                    lengthDrag = -1.0f;
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
        switch (token->tag)
        {
        case TagLogicalRadius:
            radius = token->Float();
            break;
        case TagNone:
            if (token->type == TokenKeyword && token->value == KeywordEnclosingSphere)
            {
                flags.fromBox = 1;
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
        switch (token->tag)
        {
        case TagHull:
            request.hull = token->value;
            request.allHulls = 0;
            break;
        case TagSurface:
            request.surface = token->value;
            break;
        case TagNone:
            if (token->type == TokenKeyword && token->value == KeywordAll)
            {
                request.allHulls = 1;
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
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->tag)
        {
        case TagVelocityX:
            flags.velocityGiven = 1;
            unused2 = token->Float();
            break;
        case TagVelocityY:
            flags.velocityGiven = 1;
            unused3 = token->Float();
            break;
        case TagVelocityZ:
            flags.velocityGiven = 1;
            unused4 = token->Float();
            break;
        case TagDamping:
            flags.dragGiven = 1;
            drag = token->Float();
            break;
        case TagFriction:
            flags.frictionGiven = 1;
            friction = token->Float();
            break;
        case TagBounce:
            flags.restitutionGiven = 1;
            restitution = token->Float();
            break;
        case TagNone:
            if (token->type == TokenKeyword && token->value == KeywordAutoRevert)
            {
                flags.stops = 1;
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
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        launch.designator = TokenDesignator(token, launch.designator);
        launch.space = TokenSpace(token, launch.space);
        TokenVectorComponent(token, &x);
        switch (token->tag)
        {
        case TagAgent:
            launch.receiver = token->value;
            break;
        case TagVelocityX:
            launch.velocityGiven = 1;
            launch.offsetGiven = 0;
            x = token->Float();
            break;
        case TagVelocityY:
            y = token->Float();
            launch.velocityGiven = 1;
            launch.offsetGiven = 0;
            break;
        case TagVelocityZ:
            z = token->Float();
            launch.velocityGiven = 1;
            launch.offsetGiven = 0;
            break;
        case TagDamping:
            launch.dragGiven = 1;
            drag = token->Float();
            break;
        case TagFriction:
            launch.frictionGiven = 1;
            friction = token->Float();
            break;
        case TagBounce:
            launch.restitutionGiven = 1;
            restitution = token->Float();
            break;
        case TagDiveDegreesPerSecond:
            launch.turnGiven = 1;
            turn = token->Float() * DegreesToRadians;
            break;
        case TagDuration:
            launch.thrownInTime = 1;
            heightOrTime = token->Float();
            break;
        case TagAddedRise:
            extras.addsRise = 1;
            [[fallthrough]];
        case TagRise:
            launch.thrownOverHeight = 1;
            heightOrTime = token->Float();
            break;
        case TagRouteCorner:
            extras.unused0 = 1;
            stepCorner = token->Float();
            break;
        case TagRouteToward:
            extras.unused1 = 1;
            stepToward = token->Float();
            break;
        case TagRouteScatter:
            extras.unused2 = 1;
            stepScatter = token->Float();
            break;
        case TagNone:
            if (token->type == TokenKeyword)
            {
                switch (token->value)
                {
                case KeywordAutoRevert:
                    launch.stops = 1;
                    break;
                case KeywordClampNormal:
                    launch.unused26 = 1;
                    break;
                case KeywordUnused8E:
                    extras.unused4 = 1;
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

    if (!launch.velocityGiven && !IsNearlyZero(&x))
    {
        launch.offsetGiven = 1;
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
        launch.designator = TokenDesignator(token, launch.designator);
        launch.space = TokenSpace(token, launch.space);
        TokenVectorComponent(token, &offsetX);
        switch (token->tag)
        {
        case TagAgent:
            launch.receiver = token->value;
            break;
        case TagVelocityX:
            launch.velocityGiven = 1;
            launch.offsetGiven = 0;
            offsetX = token->Float();
            break;
        case TagVelocityY:
            offsetY = token->Float();
            launch.velocityGiven = 1;
            launch.offsetGiven = 0;
            break;
        case TagVelocityZ:
            offsetZ = token->Float();
            launch.velocityGiven = 1;
            launch.offsetGiven = 0;
            break;
        case TagDuration:
            heightOrTime = token->Float();
            launch.thrownInTime = 1;
            break;
        case TagAddedRise:
            launch.addsRise = 1;
            [[fallthrough]];
        case TagRise:
            heightOrTime = token->Float();
            launch.thrownOverHeight = 1;
            break;
        case TagSpinX:
            spinX = token->Float();
            break;
        case TagSpinY:
            spinY = token->Float();
            break;
        default:
            break;
        }

        reader.Next();
    }

    if (!launch.velocityGiven && !IsNearlyZero(&offsetX))
    {
        launch.offsetGiven = 1;
    }
}

void ApplyImpulseCommand::ParseTokens(const ScriptTokenList* tokens)
{
    Vector4* impulse = reinterpret_cast<Vector4*>(&impulseX);
    *impulse = g_DefaultBox.min;
    impulse->w = 1.0f;
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->tag)
        {
        case TagAgent:
            flags.designator = token->value;
            break;
        case TagVelocityX:
            flags.impulseGiven = 1;
            impulseX = token->Float();
            break;
        case TagVelocityY:
            flags.impulseGiven = 1;
            impulseY = token->Float();
            break;
        case TagVelocityZ:
            flags.impulseGiven = 1;
            impulseZ = token->Float();
            break;
        case TagSpinX:
            flags.spinGiven = 1;
            spinX = token->Float();
            break;
        case TagSpinY:
            flags.spinGiven = 1;
            spinY = token->Float();
            break;
        case TagNone:
            if (token->type == TokenKeyword)
            {
                switch (token->value)
                {
                case KeywordFocus:
                    flags.designator = DesignatesFocus;
                    break;
                case KeywordCentre:
                    flags.unused8 = 0;
                    break;
                case KeywordContact:
                    flags.unused8 = 1;
                    break;
                case KeywordWithMass:
                    flags.unused9 = 1;
                    break;
                case KeywordFromOriginator:
                    flags.fromOriginator = 1;
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

void SetMagnetCommand::ParseTokens(const ScriptTokenList* tokens)
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
            size = token->Float();
            break;
        case TagMagnetPull:
            magnetPull = token->Float();
            break;
        case TagNone:
            if (token->type == TokenKeyword)
            {
                switch (token->value)
                {
                case KeywordMagnetMode2:
                    magnetStrength = MagnetMode2;
                    break;
                case KeywordEven:
                    magnetStrength = MagnetEven;
                    break;
                case KeywordFromBody:
                    magnetWay = MagnetFromBody;
                    break;
                case KeywordAlongZAxis:
                    magnetWay = MagnetAlongZAxis;
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

void PushInstancesAwayCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->tag)
        {
        case TagXShift:
            offsetX = token->Float();
            break;
        case TagYShift:
            offsetY = token->Float();
            break;
        case TagZShift:
            offsetZ = token->Float();
            break;
        case TagRepelForce:
            push = token->Float();
            break;
        case TagEdgePush:
            flags.byDistance = 1;
            edgePush = token->Float();
            break;
        case TagSize:
            radius = token->Float();
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
        switch (token->tag)
        {
        case TagX:
            flags.offsetGiven = 1;
            offset.x = token->Float();
            break;
        case TagY:
            flags.offsetGiven = 1;
            offset.y = token->Float();
            break;
        case TagZ:
            flags.offsetGiven = 1;
            offset.z = token->Float();
            break;
        case TagPitch:
            flags.anglesGiven = 1;
            angleX = static_cast<s32>(token->Float() * DegreesToAngle);
            break;
        case TagYaw:
            flags.anglesGiven = 1;
            angleY = static_cast<s32>(token->Float() * DegreesToAngle);
            break;
        case TagRoll:
            flags.anglesGiven = 1;
            angleZ = static_cast<s32>(token->Float() * DegreesToAngle);
            break;
        case TagExitPoint:
            flags.exitPoint = token->value;
            break;
        case TagLinked:
            flags.linked = token->value - 1;
            break;
        case TagNone:
            if (token->type == TokenKeyword)
            {
                switch (token->value)
                {
                case KeywordFirst:
                    flags.linked = 0;
                    break;
                case KeywordMakeBusy:
                    flags.marksBusy = 1;
                    break;
                case KeywordFixed:
                    flags.offsetGiven = 1;
                    break;
                case KeywordJustMove:
                    flags.follow = Attachment::FollowsPosition;
                    break;
                case KeywordSimpleChain:
                    flags.follow = Attachment::FollowsHanging;
                    break;
                case KeywordUnused7B:
                    flags.unused21 = 1;
                    break;
                case KeywordToFocus:
                    flags.toFocus = 1;
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
    offset = g_DefaultBox.min;
    offset.w = 1.0f;
    target = g_DefaultBox.min;
    target.w = 1.0f;
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        flags.space = TokenSpace(token, flags.space);
        TokenVectorComponent(token, &target.x);
        switch (token->tag)
        {
        case TagXShift:
            flags.offsetGiven = 1;
            offset.x = token->Float();
            break;
        case TagYShift:
            flags.offsetGiven = 1;
            offset.y = token->Float();
            break;
        case TagZShift:
            flags.offsetGiven = 1;
            offset.z = token->Float();
            break;
        case TagChildActor:
            flags.object = token->value;
            break;
        case TagLength:
            flags.lengthGiven = 1;
            length = token->Float();
            break;
        case TagFocusJoint:
            joints.focusJoint = token->value;
            flags.toFocus = 1;
            break;
        case TagExitPoint:
            joints.joint = token->value;
            break;
        case TagPower:
            power = token->Float();
            break;
        case TagDamping:
            damping = token->Float();
            break;
        case TagNone:
            // Whatever the token's type
            if (token->value == KeywordSpringToFocus)
            {
                flags.toFocus = 1;
            }
            else if (token->value == KeywordEndUncollidable)
            {
                flags.endUncollidable = 1;
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
        switch (token->tag)
        {
        case TagMessage:
            request.message = token->value;
            break;
        case TagExitPoint:
            request.exitPoint = token->value;
            break;
        case TagNone:
            if (token->type == TokenKeyword)
            {
                switch (token->value)
                {
                case KeywordRayDrop:
                    request.mode = DropRequest::Falls;
                    break;
                case KeywordNatural:
                    request.mode = DropRequest::Launched;
                    break;
                case KeywordContingent:
                    request.mode = DropRequest::FallsIfCrate;
                    break;
                case KeywordMakeIdle:
                    request.clearsBusy = 1;
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
        switch (token->tag)
        {
        case TagNone:
            if (token->type == TokenKeyword && token->value == KeywordMakeIdle)
            {
                release.unused16 = 1;
            }

            // Every keyword falls through into the message (no break in retail)
            [[fallthrough]];
        case TagMessage:
            release.message = token->value;
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
    ParseTaggedValueTokens(tokens, &speed);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        request.designator = TokenDesignator(token, request.designator);
        request.space = TokenSpace(token, request.space);
        TokenVectorComponent(token, &offset.x);
        switch (token->tag)
        {
        case TagExitPoint:
            request.exitPoint = token->value;
            break;
        case TagAgent:
            request.receiver = token->value;
            break;
        case TagKeyLocal:
            request.unused28 = 1;
            if (request.designator != DesignatesNone)
            {
                request.designator = request.designator + 1;
            }

            break;
        case TagTolerance:
            request.spreadGiven = 1;
            spread = token->Float();
            break;
        case TagGravity:
            ParseTaggedValueRecord(token, &gravity);
            break;
        case TagNone:
            if (token->type == TokenKeyword && token->value == KeywordMakeIdle)
            {
                request.clearsBusy = 1;
            }

            break;
        default:
            break;
        }

        reader.Next();
    }

    if (!IsNearlyZero(&offset.x))
    {
        request.offsetGiven = 1;
    }
}

void LaunchAgentRef2Command::ParseTokens(const ScriptTokenList* tokens)
{
    ParseMotionBlockTokens(tokens, &block);
    ParseTaggedValueTokens(tokens, &speed);
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        launch.designator = TokenDesignator(token, launch.designator);
        launch.space = TokenSpace(token, launch.space);
        TokenVectorComponent(token, &offset.x);
        switch (token->tag)
        {
        case TagExitPoint:
            launch.exitPoint = token->value;
            break;
        case TagAgent:
            launch.receiver = token->value;
            break;
        case TagKeyLocal:
            flags.unused0 = 1;
            if (launch.designator != DesignatesNone)
            {
                launch.designator = launch.designator + 1;
            }

            break;
        case TagTolerance:
            flags.spreadGiven = 1;
            spread = token->Float();
            break;
        case TagSpinX:
            spinX = token->Float();
            break;
        case TagSpinY:
            spinY = token->Float();
            break;
        case TagNone:
            if (token->type == TokenKeyword)
            {
                switch (token->value)
                {
                case KeywordMakeIdle:
                    flags.clearsBusy = 1;
                    break;
                case KeywordUnused76:
                    flags.unused6 = 1;
                    break;
                case KeywordPassAgentRef1:
                    flags.passesAgentRef1 = 1;
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

    if (!IsNearlyZero(&offset.x))
    {
        flags.offsetGiven = 1;
    }

    block.flags.followedWhenTouched = 1;
}

void UnsupportAboveCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    offset = g_DefaultBox.min;
    offset.w = 1.0f;
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->tag)
        {
        case TagX:
            offset.x = token->Float();
            break;
        case TagY:
            offset.y = token->Float();
            break;
        case TagZ:
            offset.z = token->Float();
            break;
        case TagAgent:
            request.receiver = token->value;
            break;
        case TagKey:
            request.designator = token->value - 1;
            break;
        case TagKeyLocal:
            request.unused20 = 1;
            if (request.designator != DesignatesNone)
            {
                request.designator = request.designator + 1;
            }

            break;
        case TagNone:
            if (token->type == TokenKeyword)
            {
                switch (token->value)
                {
                case KeywordWorldSpace:
                    request.space = ControlPacket::WorldSpace;
                    break;
                case KeywordInitialSpace:
                    request.space = ControlPacket::InitialSpace;
                    break;
                case KeywordCurrentSpace:
                    request.space = ControlPacket::CurrentSpace;
                    break;
                case KeywordTargetSpace:
                    request.space = ControlPacket::TargetSpace;
                    break;
                case KeywordStoredSpace:
                    request.space = ControlPacket::StoredSpace;
                    break;
                case KeywordCurrentKey:
                    request.designator = DesignatesCurrentKey;
                    break;
                case KeywordNextKey:
                    request.designator = DesignatesNextKey;
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

    if (!IsNearlyZero(&offset.x))
    {
        request.offsetGiven = 1;
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
        switch (token->tag)
        {
        case TagRate:
            degreesPerSecond = token->Float();
            break;
        case TagNone:
            if (token->type == TokenKeyword)
            {
                switch (token->value)
                {
                case KeywordHingeX:
                    axes.value &= ~TurnAlongAxes::TurnsMask;
                    axes.turnsX = 1;
                    break;
                case KeywordHingeY:
                    axes.value &= ~TurnAlongAxes::TurnsMask;
                    axes.turnsY = 1;
                    break;
                case KeywordHingeZ:
                    axes.value &= ~TurnAlongAxes::TurnsMask;
                    axes.turnsZ = 1;
                    break;
                case KeywordAlongX:
                    axes.value &= ~TurnAlongAxes::AlongMask;
                    axes.alongX = 1;
                    break;
                case KeywordAlongY:
                    axes.value &= ~TurnAlongAxes::AlongMask;
                    axes.alongY = 1;
                    break;
                case KeywordAlongZ:
                    axes.value &= ~TurnAlongAxes::AlongMask;
                    axes.alongZ = 1;
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
    forceX.type = TaggedValue::TypeFloat;
    forceX.SetFloat(0.0f);
    forceY.type = TaggedValue::TypeFloat;
    forceY.SetFloat(0.0f);
    forceZ.type = TaggedValue::TypeFloat;
    forceZ.SetFloat(0.0f);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->tag)
        {
        case TagX:
            ParseTaggedValueRecord(token, &forceX);
            break;
        case TagY:
            ParseTaggedValueRecord(token, &forceY);
            break;
        case TagZ:
            ParseTaggedValueRecord(token, &forceZ);
            break;
        case TagNone:
            if (token->type == TokenKeyword)
            {
                // Any other keyword clears both
                turns.on = token->value == KeywordOn ? 1 : 0;
                turns.off = token->value == KeywordOff ? 1 : 0;
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
        target.recipient = TokenDesignator(token, target.recipient);
        switch (token->tag)
        {
        case TagMessage:
            target.message = token->value;
            break;
        case TagLinked:
            flags.byIndex = 1;
            target.recipient = token->value - 1;
            break;
        case TagActor:
            flags.object = token->value;
            break;
        case TagField:
            flags.field = token->value;
            flags.byField = 1;
            break;
        case TagFieldBits:
            fieldBits.type = TaggedValue::TypeInt;
            ParseTaggedValueRecord(token, &fieldBits);
            flags.byField = 1;
            break;
        case TagFieldWidth:
            fieldWidth.type = TaggedValue::TypeInt;
            ParseTaggedValueRecord(token, &fieldWidth);
            flags.byField = 1;
            break;
        case TagNone:
            if (token->type == TokenKeyword)
            {
                switch (token->value)
                {
                case KeywordFocus:
                    target.recipient = DesignatesFocus;
                    break;
                case KeywordLinkedById:
                    target.recipient = DesignatesLinkedById;
                    break;
                case KeywordFirst:
                    target.recipient = 0;
                    flags.byIndex = 1;
                    break;
                case KeywordAll:
                    flags.byIndex = 1;
                    flags.everyLinked = 1;
                    break;
                case KeywordCurrentLinked:
                    flags.byIndex = 1;
                    flags.currentLinked = 1;
                    break;
                case KeywordUnlink:
                    flags.unlinks = 1;
                    break;
                case KeywordLast:
                    flags.byIndex = 1;
                    flags.lastLinked = 1;
                    break;
                case KeywordOffPath:
                    flags.onlyOffPath = 1;
                    break;
                case KeywordOnPath:
                    flags.onlyOnPath = 1;
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
    offset = g_DefaultBox.min;
    offset.w = 1.0f;
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        target.designator = TokenDesignator(token, target.designator);
        target.space = TokenSpace(token, target.space);
        TokenVectorComponent(token, &offset.x);
        switch (token->tag)
        {
        case TagMessage:
            target.message = token->value;
            break;
        case TagAgent:
            target.receiver = token->value;
            break;
        case TagActor:
            object.id = token->value;
            break;
        case TagRange:
            radius = token->Float();
            break;
        case TagNone:
            if (token->type == TokenKeyword && token->value == KeywordFocus)
            {
                target.receiver = DesignatesFocus;
            }

            break;
        default:
            break;
        }

        reader.Next();
    }

    if (!IsNearlyZero(&offset.x))
    {
        target.offsetGiven = 1;
    }
}

void TriggerInstancesByRankCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->tag)
        {
        case TagMessage:
            message.message = token->value;
            break;
        case TagNone:
            if (token->type == TokenKeyword)
            {
                switch (token->value)
                {
                case KeywordSameRank:
                    message.mode = RankMessage::SameRank;
                    break;
                case KeywordBelowTriggerRank:
                    message.mode = RankMessage::BelowTriggerRank;
                    break;
                case KeywordUpToTriggerRank:
                    message.mode = RankMessage::UpToTriggerRank;
                    break;
                case KeywordTriggerRank:
                    message.mode = RankMessage::TriggerRank;
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

void SetNoiseMessageCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->tag)
        {
        case TagMessage:
            settings.message = token->value;
            break;
        case TagNone:
            if (token->type == TokenKeyword)
            {
                if (token->value == KeywordOn)
                {
                    settings.passesNoises = 1;
                }
                else if (token->value == KeywordOff)
                {
                    settings.passesNoises = 0;
                }
            }

            break;
        default:
            break;
        }

        reader.Next();
    }
}

void SetBodyMassCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        if (token->tag == TagMass)
        {
            mass = token->Float();
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
        if (token->tag == TagGravity)
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
        if (token->tag == TagRange)
        {
            ParseTaggedValueRecord(token, &radius);
        }

        reader.Next();
    }
}

void SetContactSpringyCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ParseMotionBlockTokens(tokens, &block);
    block.flags.followedWhenTouched = 1;
}

void AttachMotionBlockCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        if (token->tag == TagMessage)
        {
            touchMessage = static_cast<s32>(token->value);
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
        switch (token->tag)
        {
        case TagRange:
            radius = token->Float();
            break;
        case TagLoudness:
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
        if (token->tag == TagMessage)
        {
            message.id = token->value;
        }

        reader.Next();
    }
}
