#include "game/commands.h"

#include "game/camerarig.h"
#include "game/math.h"
#include "game/navigation.h"
#include "game/nodecontrollers.h"
#include "game/progress.h"
#include "game/scripttokens.h"

// The commands' development tools parsers (their vtables' slot 2): each token's tag names an argument, its value goes into the
// command's field. The retail game never calls them

namespace
{
// The kinds of nodes RequestFocus's keywords take or rule out (a bit per NodeKind): the players' are three
constexpr u32 PlayerKinds = 1u << NodeControls | 1u << NodeCharacter | 1u << NodeFollow;

// The AI positions' flags GetShortRoute's keywords rule out of its start and end (game/navigation.h's AiPositionFlags)
constexpr u32 FlaggedPositions =
    AiPositionFlags::Airborne | AiPositionFlags::AlwaysTaken | AiPositionFlags::NeverTaken | AiPositionFlags::ScriptFlag6;

// Where the bottom text's centre keyword puts it
constexpr f32 BottomCentreX = 0.5f;
constexpr f32 BottomCentreY = Rounded(0.92);
}

void SetChiChiGrassCommand::ParseTokens(const ScriptTokenList* tokens)
{
    // On sets it, Off clears it
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        if (token->tag == TagSelectable)
        {
            if (token->value == KeywordOn)
            {
                setting.given = 1;
                setting.on = 1;
            }
            else if (token->value == KeywordOff)
            {
                setting.given = 1;
                setting.on = 0;
            }
        }

        reader.Next();
    }
}

void NoOpSetRayTestsCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->tag)
        {
        case TagWallRadius:
            unused1 = static_cast<s32>(token->value);
            break;
        case TagCliffRadius:
            unused2 = static_cast<s32>(token->value);
            break;
        case TagWallShy:
            unused3 = static_cast<s32>(token->value);
            break;
        case TagCliffShy:
            unused4 = static_cast<s32>(token->value);
            break;
        case TagYRaise:
            unused5 = static_cast<s32>(token->value);
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
        switch (token->tag)
        {
        case TagNone:
            if (token->type == TokenKeyword)
            {
                switch (token->value)
                {
                case KeywordGetCrates:
                    kinds |= 1 << NodeCrate;
                    break;
                case KeywordGetCreatures:
                    kinds |= 1 << NodeCreature;
                    break;
                case KeywordGetCharacters:
                    kinds |= 1 << NodeCharacter;
                    break;
                case KeywordGetFurniture:
                    kinds |= 1 << NodeGenericObject;
                    break;
                default:
                    break;
                }
            }

            break;
        case TagStickStrength:
            strength = token->Float();
            break;
        case TagActor:
            object.object = token->value;
            break;
        case TagMessage:
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
        if (token->tag == TagNone && token->type == TokenKeyword)
        {
            if (token->value == KeywordOn)
            {
                staysSticky.on = 1;
            }
            else if (token->value == KeywordOff)
            {
                staysSticky.on = 0;
            }
        }

        reader.Next();
    }
}

void SetAttacksTakenCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->tag)
        {
        case TagSpinAttack:
            settings.spin = TokenSetting(token);
            break;
        case TagUnused240:
            settings.unused6 = TokenSetting(token);
            break;
        case TagUnused23D:
            settings.unused2 = TokenSetting(token);
            break;
        case TagSlamAttack:
            settings.slam = TokenSetting(token);
            break;
        case TagWalkIntoAttack:
            settings.walkInto = TokenSetting(token);
            break;
        default:
            break;
        }

        reader.Next();
    }
}

void SwitchBodyFlagsCommand::ParseTokens(const ScriptTokenList* tokens)
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
            switches.unused0 = TokenIsOn(token) ? SwitchOn : SwitchOff;
            break;
        case TagHitCreatures:
            switches.unused2 = TokenIsOn(token) ? SwitchOn : SwitchOff;
            break;
        case TagHitFurniture:
            switches.unused4 = TokenIsOn(token) ? SwitchOn : SwitchOff;
            break;
        case TagHitPlayer:
            switches.unused6 = TokenIsOn(token) ? SwitchOn : SwitchOff;
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
        switch (token->tag)
        {
        case TagRedWumpaActor:
            contents.first = token->value;
            break;
        case TagHealthActor:
            contents.second = token->value;
            break;
        case TagMinRedWumpa:
            count.least = token->value;
            break;
        case TagMaxRedWumpa:
            count.most = token->value;
            break;
        default:
            break;
        }

        reader.Next();
    }
}

void CountPlayerApproachCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->tag)
        {
        case TagTurn:
            counter.unused17 = 0;
            break;
        case TagCounter:
            counter.counter = token->value;
            break;
        case TagAgentCounter:
            counter.counter = token->value;
            counter.agentCounter = 1;
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
        if (token->tag == TagNone && token->type == TokenKeyword)
        {
            if (token->value == KeywordPlayer)
            {
                choice.played = 1;
            }
            else
            {
                choice.character = TokenCharacter(token->value);
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
        switch (token->tag)
        {
        case TagNone:
            switch (token->value)
            {
            case KeywordHitExplosion:
                hitKinds |= HitExplosion;
                break;
            case KeywordHitFallingThrough:
                hitKinds |= HitFallingThrough;
                break;
            case KeywordHitBurning:
                hitKinds |= HitBurning;
                break;
            case KeywordHitIceteroid:
                hitKinds |= HitIceteroid;
                break;
            case KeywordHitProjectile:
                hitKinds |= HitProjectile;
                break;
            case KeywordHitKind6:
                hitKinds |= HitKind6;
                break;
            case KeywordHitElectric:
                hitKinds |= HitElectric;
                break;
            case KeywordHitKind8:
                hitKinds |= HitKind8;
                break;
            case KeywordHitCrush:
                hitKinds |= HitCrush;
                break;
            case KeywordHitKind12:
                hitKinds |= HitKind12;
                break;
            case KeywordHitKind13:
                hitKinds |= HitKind13;
                break;
            case KeywordHitKind14:
                hitKinds |= HitKind14;
                break;
            case KeywordHitBite:
                hitKinds |= HitBite;
                break;
            case KeywordHitSpin:
                hitKinds |= HitSpin;
                break;
            case KeywordHitKick:
                hitKinds |= HitKick;
                break;
            case KeywordHitKneeDrop:
                hitKinds |= HitKneeDrop;
                break;
            case KeywordHitKind18:
                hitKinds |= HitKind18;
                break;
            case KeywordHitHeavy:
                hitKinds |= HitHeavy;
                break;
            case KeywordHitKind22:
                hitKinds |= HitKind22;
                break;
            case KeywordHitKind21:
                hitKinds |= HitKind21;
                break;
            case KeywordHitSinking:
                hitKinds |= HitSinking;
                break;
            default:
                break;
            }

            break;
        case TagHitPoints:
            ParseTaggedValueRecord(token, &damage);
            break;
        default:
            break;
        }

        reader.Next();
    }
}

void HitInstancesInBoxesCommand::ParseTokens(const ScriptTokenList* tokens)
{
    // The keywords of the kinds of hit are DamageOriginator's
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->tag)
        {
        case TagMessage:
            flags.sendsMessage = 1;
            message.value = static_cast<u16>(token->value);
            break;
        case TagNone:
            switch (token->value)
            {
            case KeywordNoCharacters:
                flags.skipsCharacters = 1;
                break;
            case KeywordOnlyCharacters:
                flags.onlyCharacters = 1;
                break;
            case KeywordHitExplosion:
                hitKinds |= HitExplosion;
                break;
            case KeywordHitFallingThrough:
                hitKinds |= HitFallingThrough;
                break;
            case KeywordHitBurning:
                hitKinds |= HitBurning;
                break;
            case KeywordHitIceteroid:
                hitKinds |= HitIceteroid;
                break;
            case KeywordHitProjectile:
                hitKinds |= HitProjectile;
                break;
            case KeywordHitKind6:
                hitKinds |= HitKind6;
                break;
            case KeywordHitElectric:
                hitKinds |= HitElectric;
                break;
            case KeywordHitKind8:
                hitKinds |= HitKind8;
                break;
            case KeywordHitCrush:
                hitKinds |= HitCrush;
                break;
            case KeywordHitKind12:
                hitKinds |= HitKind12;
                break;
            case KeywordHitKind13:
                hitKinds |= HitKind13;
                break;
            case KeywordHitKind14:
                hitKinds |= HitKind14;
                break;
            case KeywordHitBite:
                hitKinds |= HitBite;
                break;
            case KeywordHitSpin:
                hitKinds |= HitSpin;
                break;
            case KeywordHitKick:
                hitKinds |= HitKick;
                break;
            case KeywordHitKneeDrop:
                hitKinds |= HitKneeDrop;
                break;
            case KeywordHitKind18:
                hitKinds |= HitKind18;
                break;
            case KeywordHitHeavy:
                hitKinds |= HitHeavy;
                break;
            case KeywordHitKind22:
                hitKinds |= HitKind22;
                break;
            case KeywordHitKind21:
                hitKinds |= HitKind21;
                break;
            case KeywordHitSinking:
                hitKinds |= HitSinking;
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

void SwingAroundCameraCommand::ParseTokens(const ScriptTokenList* tokens)
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
            circleX = token->Float();
            break;
        case TagY:
            circleY = token->Float();
            break;
        case TagZ:
            depth = token->Float();
            break;
        case TagScale:
            heightRate = token->Float();
            break;
        case TagRange:
            heightLimit = token->Float();
            break;
        case TagLowest:
            nearDistanceSquared = token->Float();
            break;
        case TagHighest:
            farDistanceSquared = token->Float();
            break;
        case TagXPhase:
            phaseOffset = token->Float();
            break;
        case TagYPhase:
            unused2C = token->value;
            break;
        case TagXMagnitude:
            unused30 = token->Float();
            break;
        case TagYMagnitude:
            unused34 = token->value;
            break;
        case TagSenseInterval:
            phaseScale = token->Float();
            break;
        default:
            break;
        }

        reader.Next();
    }

    // Most of what the tokens gave is then replaced by random and fixed values: the radius and the depth a unit either way of
    // these, the squared distances, the height and its limit
    constexpr f32 MiddleRadius = 2.0f;
    constexpr f32 FixedNearDistanceSquared = 150.0f;
    constexpr f32 FixedFarDistanceSquared = 200.0f;
    constexpr f32 FixedHeight = -2.0f;
    constexpr f32 FixedHeightLimit = 2.0f;
    constexpr f32 MiddleDepth = 5.5f;
    phaseOffset = RandomSignedTimes(Pi);
    unused30 = RandomSignedTimes(0.25f) + 1.0f;
    phaseScale = RandomSignedTimes(0.5f) + 0.5f;
    radius = RandomSignedTimes(1.0f) + MiddleRadius;
    nearDistanceSquared = FixedNearDistanceSquared;
    farDistanceSquared = FixedFarDistanceSquared;
    heightRate = heightRate * 0.5f;
    height = FixedHeight;
    heightLimit = FixedHeightLimit;
    depth = RandomSignedTimes(1.0f) + MiddleDepth;
}

void CutsceneCameraMoveCommand::ParseTokens(const ScriptTokenList* tokens)
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
            pitch = token->Float();
            break;
        case TagExtraDistance:
            extraDistance = token->Float();
            break;
        case TagExtraHeight:
            extraHeightShare = token->Float();
            break;
        case TagCameraYaw:
            yaw = token->Float();
            break;
        case TagTargetSeconds:
            targetSeconds = token->Float();
            break;
        case TagCameraSeconds:
            cameraSeconds = token->Float();
            break;
        case TagUnused245:
            unused28 = token->value;
            break;
        case TagUnused246:
            unused2C = token->value;
            break;
        case TagFov:
            framing.fovGiven = 1;
            fov = static_cast<s32>(token->Float() * DegreesToAngle);
            break;
        case TagTargetAlong:
            targetAlong = token->Float();
            break;
        case TagCameraAlong:
            cameraAlong = token->Float();
            break;
        case TagNone:
            if (token->type == TokenKeyword)
            {
                switch (token->value)
                {
                case KeywordFixedYaw145:
                    framing.fixedYaw = 0;
                    break;
                case KeywordFixedYaw15:
                    framing.fixedYaw = 1;
                    break;
                case KeywordFixedYaw180:
                    framing.fixedYaw = 2;
                    break;
                case KeywordFixedYaw90:
                    framing.fixedYaw = 3;
                    break;
                case KeywordNoFixedYaw:
                    framing.fixedYaw = 4;
                    break;
                case KeywordShot0:
                    framing.shot = 0;
                    break;
                case KeywordShot1:
                    framing.shot = 1;
                    break;
                case KeywordShot2:
                    framing.shot = 2;
                    break;
                case KeywordShot3:
                    framing.shot = 3;
                    break;
                case KeywordShot4:
                    framing.shot = 4;
                    break;
                case KeywordShot5:
                    framing.shot = 5;
                    break;
                case KeywordShot6:
                    framing.shot = 6;
                    break;
                case KeywordShot7:
                    framing.shot = 7;
                    break;
                case KeywordNoFramingAngles:
                    framing.angles = 0;
                    break;
                case KeywordFramingSixth:
                    framing.angles = 1;
                    break;
                case KeywordFramingThird:
                    framing.angles = 2;
                    break;
                case KeywordFramingHalf:
                    framing.angles = 3;
                    break;
                case KeywordAimFirst:
                    framing.aim = AimFirst;
                    break;
                case KeywordAimSecond:
                    framing.aim = AimSecond;
                    break;
                case KeywordAimBetween:
                    framing.aim = AimBetween;
                    break;
                case KeywordDistanceFromFirst:
                    framing.distanceFrom = AimFirst;
                    break;
                case KeywordDistanceFromSecond:
                    framing.distanceFrom = AimSecond;
                    break;
                case KeywordArc:
                    framing.arcs = 1;
                    break;
                case KeywordEaseIn:
                    framing.easesIn = 1;
                    break;
                case KeywordEaseOut:
                    framing.easesOut = 1;
                    break;
                case KeywordEven:
                    framing.curve = MoveEven;
                    break;
                case KeywordSmooth:
                    framing.curve = MoveSmooth;
                    break;
                case KeywordUnused26D:
                    framing.unused22 = 1;
                    break;
                case KeywordUnused26E:
                    framing.unused23 = 1;
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
        switch (token->tag)
        {
        case TagBlendSeconds:
            blendTime = token->Float();
            break;
        case TagNone:
            if (token->type == TokenKeyword)
            {
                switch (token->value)
                {
                case KeywordFollowCamera:
                    mode.shown = ShowsFollowCamera;
                    break;
                case KeywordGameRig:
                    mode.shown = ShowsGameRig;
                    break;
                case KeywordCutsceneRig:
                    mode.shown = ShowsCutsceneRig;
                    break;
                case KeywordReset:
                    mode.resets = 1;
                    break;
                case KeywordEven:
                    mode.curve = CurveEven;
                    break;
                case KeywordSmooth:
                    mode.curve = CurveSmooth;
                    break;
                case KeywordPlaceFollowCamera:
                    mode.placesFollowCamera = 1;
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
        switch (token->tag)
        {
        case TagFirstTarget:
            designators.first = TokenDesignator(token, designators.first);
            break;
        case TagSecondTarget:
            designators.second = TokenDesignator(token, designators.second);
            break;
        case TagFirstTargetPosition:
            designators.firstPosition = TokenDesignator(token, designators.firstPosition);
            break;
        case TagSecondTargetPosition:
            designators.secondPosition = TokenDesignator(token, designators.secondPosition);
            break;
        case TagTargetPath:
            paths.target = token->value;
            break;
        case TagCameraPath:
            paths.camera = token->value;
            break;
        case TagNone:
            if (token->type == TokenKeyword)
            {
                if (token->value == KeywordMirrored)
                {
                    flags.mirrored = 1;
                }
                else if (token->value == KeywordNoFraming)
                {
                    flags.frameWanted = 0;
                }
            }

            break;
        default:
            break;
        }

        reader.Next();
    }
}

void SetFollowCameraRateCommand::ParseTokens(const ScriptTokenList* tokens)
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
            if (token->type == TokenKeyword)
            {
                if (token->value == KeywordPositionRate)
                {
                    which.kind = FollowCameraRate::PositionRate;
                }
                else if (token->value == KeywordYawSpeed)
                {
                    which.kind = FollowCameraRate::YawSpeed;
                }
            }

            break;
        case TagRate:
            rate = token->Float();
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
        switch (token->tag)
        {
        case TagNone:
            if (token->type == TokenKeyword)
            {
                switch (token->value)
                {
                case KeywordCameraPitch:
                    mode.which = ValuePitch;
                    break;
                case KeywordCameraDistance:
                    mode.which = ValueDistance;
                    break;
                case KeywordCameraYaw:
                    mode.which = ValueYaw;
                    break;
                case KeywordCameraFov:
                    mode.which = ValueFov;
                    break;
                default:
                    break;
                }
            }

            break;
        case TagLowest:
            start = token->Float();
            break;
        case TagHighest:
            end = token->Float();
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
        switch (token->tag)
        {
        case TagPlayerMode:
            pairing = TokenPlayerMode(token->value);
            break;
        case TagFirstCharacter:
            character = TokenCharacter(token->value);
            break;
        case TagSecondCharacter:
            second = TokenCharacter(token->value);
            break;
        default:
            break;
        }

        reader.Next();
    }
}

void FadeoutScreenCommand::ParseTokens(const ScriptTokenList* tokens)
{
    // A colour that isn't 0 is set
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->tag)
        {
        case TagNone:
            if (token->type == TokenKeyword)
            {
                if (token->value == KeywordIn)
                {
                    flags.mode = ModeHide;
                }
                else if (token->value == KeywordOut)
                {
                    flags.mode = ModeShow;
                }
            }

            break;
        case TagDuration:
            duration = token->Float();
            break;
        case TagScale:
            unused14 = static_cast<s32>(token->value);
            break;
        case TagRed:
            red = token->Float();
            flags.setsColour |= __builtin_fabsf(red) <= Epsilon ? 0u : 1u;
            break;
        case TagGreen:
            green = token->Float();
            flags.setsColour |= __builtin_fabsf(green) <= Epsilon ? 0u : 1u;
            break;
        case TagBlue:
            blue = token->Float();
            flags.setsColour |= __builtin_fabsf(blue) <= Epsilon ? 0u : 1u;
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
        switch (token->tag)
        {
        case TagNone:
            if (token->type == TokenKeyword)
            {
                switch (token->value)
                {
                case KeywordRed:
                    red = 1.0f;
                    break;
                case KeywordGreen:
                    green = 1.0f;
                    break;
                case KeywordBlue:
                    blue = 1.0f;
                    break;
                case KeywordCyan:
                    green = 1.0f;
                    blue = 1.0f;
                    break;
                case KeywordMagenta:
                    red = 1.0f;
                    blue = 1.0f;
                    break;
                case KeywordYellow:
                    red = 1.0f;
                    green = 1.0f;
                    break;
                case KeywordBlack:
                    red = 0.0f;
                    green = 0.0f;
                    blue = 0.0f;
                    break;
                case KeywordWhite:
                    red = 1.0f;
                    green = 1.0f;
                    blue = 1.0f;
                    break;
                case KeywordBottomCentre:
                    x = BottomCentreX;
                    y = BottomCentreY;
                    break;
                default:
                    break;
                }
            }

            break;
        case TagText:
            text = static_cast<s32>(token->value);
            break;
        case TagX:
            x = token->Float();
            break;
        case TagY:
            y = token->Float();
            break;
        case TagRed:
            red = token->Float();
            break;
        case TagGreen:
            green = token->Float();
            break;
        case TagBlue:
            blue = token->Float();
            break;
        case TagDuration:
            seconds = token->Float();
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
        switch (token->tag)
        {
        case TagNone:
            if (token->type == TokenKeyword)
            {
                switch (token->value)
                {
                case KeywordRed:
                    red = 1.0f;
                    break;
                case KeywordGreen:
                    green = 1.0f;
                    break;
                case KeywordBlue:
                    blue = 1.0f;
                    break;
                case KeywordCyan:
                    green = 1.0f;
                    blue = 1.0f;
                    break;
                case KeywordMagenta:
                    red = 1.0f;
                    blue = 1.0f;
                    break;
                case KeywordYellow:
                    red = 1.0f;
                    green = 1.0f;
                    break;
                case KeywordBlack:
                    red = 0.0f;
                    green = 0.0f;
                    blue = 0.0f;
                    break;
                case KeywordWhite:
                    red = 1.0f;
                    green = 1.0f;
                    blue = 1.0f;
                    break;
                case KeywordBottomCentre:
                    x = BottomCentreX;
                    y = BottomCentreY;
                    break;
                default:
                    break;
                }
            }

            break;
        case TagX:
            x = token->Float();
            break;
        case TagY:
            y = token->Float();
            break;
        case TagRed:
            red = token->Float();
            break;
        case TagGreen:
            green = token->Float();
            break;
        case TagBlue:
            blue = token->Float();
            break;
        case TagDuration:
            seconds = token->Float();
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
        switch (token->tag)
        {
        case TagNone:
            if (token->type != TokenInt)
            {
                break;
            }

            [[fallthrough]];
        case TagValue:
            movie.type = TaggedValue::TypeInt;
            ParseTaggedValueRecord(token, &movie);
            break;
        case TagDuration:
            delay = token->Float();
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
        switch (token->tag)
        {
        case TagNone:
            if (token->type == TokenKeyword)
            {
                switch (token->value)
                {
                case KeywordInitialSpace:
                    space = ControlPacket::InitialSpace;
                    break;
                case KeywordCurrentSpace:
                    space = ControlPacket::CurrentSpace;
                    break;
                case KeywordTargetSpace:
                    space = ControlPacket::TargetSpace;
                    break;
                default:
                    space = ControlPacket::WorldSpace;
                    break;
                }
            }

            break;
        case TagTolerance:
            distance = -token->Float();
            break;
        case TagSize:
            unused7 = static_cast<s32>(token->value);
            break;
        case TagX:
            vectorX = token->Float();
            break;
        case TagY:
            vectorY = token->Float();
            break;
        case TagZ:
            vectorZ = token->Float();
            break;
        case TagFromDesignator:
            target.designator = TokenDesignator(token, target.designator);
            break;
        default:
            break;
        }

        reader.Next();
    }

    x = vectorX;
    y = vectorY;
    z = vectorZ;
    w = 1.0f;
}

void SetFocusPositionToNearestPointCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->tag)
        {
        case TagAhead:
            unused2 = token->Float();
            target.unused8 = 1;
            break;
        case TagAgent:
            target.receiver = token->value;
            break;
        default:
            break;
        }

        reader.Next();
    }
}

void RequestFocusCommand::ParseTokens(const ScriptTokenList* tokens)
{
    // The first keyword that sets kinds clears the ones it had
    bool clearKinds = true;
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
        case TagNone:
            if (token->type == TokenKeyword)
            {
                switch (token->value)
                {
                case KeywordHoldSomething:
                    target.attachment = Holding;
                    break;
                case KeywordHoldNothing:
                    target.attachment = HoldingNothing;
                    break;
                case KeywordHeld:
                    target.attachment = Hanging;
                    break;
                case KeywordUnheld:
                    target.attachment = HangingFromNothing;
                    break;
                case KeywordBusy:
                    target.busy = OnlyBusy;
                    break;
                case KeywordMakeAgentRef2:
                    target.alsoAgentRef2 = 1;
                    break;
                case KeywordIdle:
                    target.busy = NoneBusy;
                    break;
                case KeywordMakeBusy:
                    target.marksBusy = 1;
                    break;
                case KeywordVisibleOnly:
                    choice.visibleOnly = 1;
                    break;
                case KeywordRandomChoice:
                    target.nearest = 0;
                    target.random = 1;
                    break;
                case KeywordNoCrates:
                    kinds &= ~(1u << NodeCrate);
                    break;
                case KeywordNoCreatures:
                    kinds &= ~(1u << NodeCreature);
                    break;
                case KeywordNoFurniture:
                    kinds &= ~(1u << NodeGenericObject);
                    break;
                case KeywordNoPlayers:
                    kinds &= ~PlayerKinds;
                    break;
                case KeywordNoPickups:
                    kinds &= ~(1u << NodePickup);
                    break;
                case KeywordNoFoofie:
                    kinds &= ~(1u << NodeGraple);
                    break;
                case KeywordNoChiChi:
                    kinds &= ~(1u << NodeGrabbable);
                    break;
                case KeywordNoProjectiles:
                    kinds &= ~(1u << NodeProjectile);
                    break;
                case KeywordNoPayGates:
                    kinds &= ~(1u << NodePayGate);
                    break;
                case KeywordGetCrates:
                    if (clearKinds)
                    {
                        kinds = 0;
                        clearKinds = false;
                    }

                    kinds |= 1u << NodeCrate;
                    break;
                case KeywordGetCreatures:
                    if (clearKinds)
                    {
                        kinds = 0;
                        clearKinds = false;
                    }

                    kinds |= 1u << NodeCreature;
                    break;
                case KeywordGetPlayers:
                    if (clearKinds)
                    {
                        kinds = 0;
                        clearKinds = false;
                    }

                    kinds |= PlayerKinds;
                    break;
                case KeywordGetProjectiles:
                    if (clearKinds)
                    {
                        kinds = 0;
                        clearKinds = false;
                    }

                    kinds |= 1u << NodeProjectile;
                    break;
                case KeywordGetPickups:
                    if (clearKinds)
                    {
                        kinds = 0;
                        clearKinds = false;
                    }

                    kinds |= 1u << NodePickup;
                    break;
                case KeywordGetFoofie:
                    if (clearKinds)
                    {
                        kinds = 0;
                        clearKinds = false;
                    }

                    kinds |= 1u << NodeGraple;
                    break;
                case KeywordGetChiChi:
                    if (clearKinds)
                    {
                        kinds = 0;
                        clearKinds = false;
                    }

                    kinds |= 1u << NodeGrabbable;
                    break;
                case KeywordGetPayGates:
                    if (clearKinds)
                    {
                        kinds = 0;
                        clearKinds = false;
                    }

                    kinds |= 1u << NodePayGate;
                    break;
                case KeywordIgnoreSignals:
                    choice.ignoresSignals = 1;
                    break;
                case KeywordKeepCurrent:
                    choice.keepsCurrent = 1;
                    break;
                default:
                    break;
                }
            }

            break;
        case TagHoldingActor:
        {
            // The hanging instances' objects (past the second, into the target word)
            u32 count = choice.hangingCount;
            choice.hangingCount = count + 1;
            objects[HangingObjects + count] = token->value;
            target.attachment = HangingFromObjects;
            break;
        }
        case TagAgent:
            target.receiver = token->value;
            break;
        case TagActor:
        {
            // The objects (past the sixth, into the target word)
            u32 count = target.objectCount;
            target.objectCount = count + 1;
            objects[count] = token->value;
            break;
        }
        case TagRange:
            radius = token->Float();
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

void SetFocusPropertiesCommand::ParseTokens(const ScriptTokenList* tokens)
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
            properties.awake = TokenSetting(token);
            break;
        case TagVisible:
            properties.visible = TokenSetting(token);
            break;
        case TagCollidable:
            properties.collisionActive = TokenSetting(token);
            break;
        case TagTangible:
            properties.receivesTriggerSignals = TokenSetting(token);
            break;
        case TagHarmful:
            properties.canDamageCharacter = TokenSetting(token);
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
        switch (token->tag)
        {
        case TagNone:
            switch (token->value)
            {
            case KeywordSteerInstance:
                settings.bits.steering |= HeadTrackingSettings::SteersInstance;
                break;
            case KeywordSteerFacing:
                settings.bits.steering |= HeadTrackingSettings::SteersFacing;
                break;
            case KeywordIgnoreNoises:
                settings.bits.ignoresNoises = 1;
                break;
            default:
                break;
            }

            break;
        case TagRange:
            settings.range = token->Float() * token->Float();
            break;
        case TagUnusedB9:
            settings.unused00 = token->value;
            break;
        case TagTrackingStiffness:
            settings.stiffness = token->Float();
            break;
        case TagYawLimit:
            SetHeadTrackingYawLimit(&settings, token->Float());
            break;
        case TagNegativePitchLimit:
            SetHeadTrackingNegativePitch(&settings, token->Float());
            break;
        case TagPositivePitchLimit:
            SetHeadTrackingPositivePitch(&settings, token->Float());
            break;
        case TagUnusedBA:
            settings.unused24[0] = token->value;
            break;
        case TagUnusedBB:
            settings.unused24[1] = token->value;
            break;
        case TagUnusedBC:
            settings.unused24[2] = token->value;
            break;
        case TagUnusedBD:
            settings.unused24[3] = token->value;
            break;
        case TagUnusedBE:
            settings.unused24[4] = token->value;
            break;
        case TagUnusedBF:
            settings.unused24[5] = token->value;
            break;
        case TagHeadJoint:
            settings.bits.joint = token->value;
            break;
        case TagSecondHeadJoint:
            settings.bits.secondJoint = token->value;
            break;
        case TagExitPoint:
            settings.bits.exitPoint = token->value;
            break;
        case TagDamping:
            settings.damping = token->Float();
            break;
        case TagLodBoost:
            settings.unseenLimit = token->value * token->value;
            break;
        default:
            break;
        }

        reader.Next();
    }
}

void SetKeyNearestPlayerCommand::ParseTokens(const ScriptTokenList* tokens)
{
    // A distance of this or less is taken for none
    constexpr f32 LeastDistance = Rounded(1e-05);
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->tag)
        {
        case TagTolerance:
            if (LeastDistance < token->Float())
            {
                nearDistanceSquared = token->Float() * token->Float();
            }

            break;
        case TagLeadSeconds:
            leadSeconds = token->Float();
            break;
        case TagMinKey:
            keys.first = token->value - 1;
            break;
        case TagMaxKey:
            keys.last = token->value - 1;
            break;
        case TagNone:
            if (token->value == KeywordKeepCurrent)
            {
                keys.takesCurrent = 1;
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
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        target.space = TokenSpace(token, target.space);
        target.designator = TokenDesignator(token, target.designator);
        TokenVectorComponent(token, &offsetX);
        switch (token->tag)
        {
        case TagNone:
            switch (token->value)
            {
            case KeywordNoPathFlag6:
                options.takesPathFlag6 = 0;
                target.distanceOnly = 0;
                break;
            case KeywordNoPlainPaths:
                options.takesPlainPaths = 0;
                target.distanceOnly = 0;
                break;
            case KeywordNoJumps:
                target.takesJumps = 0;
                target.distanceOnly = 0;
                break;
            case KeywordNoLongJumps:
                target.takesLongJumps = 0;
                target.distanceOnly = 0;
                if (!target.takesHighJumps)
                {
                    target.takesJumps = 0;
                }

                break;
            case KeywordNoHighJumps:
                target.takesHighJumps = 0;
                target.distanceOnly = 0;
                if (!target.takesLongJumps)
                {
                    target.takesJumps = 0;
                }

                break;
            case KeywordNoPathFlag8:
                options.takesPathFlag8 = 0;
                target.distanceOnly = 0;
                break;
            case KeywordNoPathFlag7:
                options.takesPathFlag7 = 0;
                target.distanceOnly = 0;
                break;
            case KeywordNoFlights:
                target.takesFlights = 0;
                target.distanceOnly = 0;
                break;
            case KeywordOnlyPathFlag6:
                options.takesPathFlag7 = 0;
                options.takesPathFlag8 = 0;
                options.takesPlainPaths = 0;
                target.takesJumps = 0;
                target.takesFlights = 0;
                target.distanceOnly = 0;
                break;
            case KeywordPlainAndJumpsOnly:
                options.takesPathFlag6 = 0;
                options.takesPathFlag7 = 0;
                options.takesPathFlag8 = 0;
                target.takesFlights = 0;
                target.distanceOnly = 0;
                break;
            case KeywordOnlyFlights:
                options.takesPathFlag6 = 0;
                options.takesPathFlag7 = 0;
                options.takesPathFlag8 = 0;
                options.takesPlainPaths = 0;
                target.takesJumps = 0;
                target.distanceOnly = 0;
                break;
            case KeywordEndUnflagged:
                positionFlags.endRuledOut |= FlaggedPositions;
                break;
            case KeywordEndAlwaysTaken:
                endFlags.endRequired |= AiPositionFlags::AlwaysTaken;
                break;
            case KeywordEndAirborne:
                endFlags.endRequired |= AiPositionFlags::Airborne;
                break;
            case KeywordEndFlag4:
                endFlags.endRequired |= AiPositionFlags::NeverTaken;
                break;
            case KeywordEndFlag6:
                endFlags.endRequired |= AiPositionFlags::ScriptFlag6;
                break;
            case KeywordStartUnflagged:
                startFlags.startRuledOut |= FlaggedPositions;
                break;
            case KeywordStartAlwaysTaken:
                positionFlags.startRequired |= AiPositionFlags::AlwaysTaken;
                break;
            case KeywordStartAirborne:
                positionFlags.startRequired |= AiPositionFlags::Airborne;
                break;
            case KeywordStartFlag4:
                positionFlags.startRequired |= AiPositionFlags::NeverTaken;
                break;
            case KeywordStartFlag6:
                positionFlags.startRequired |= AiPositionFlags::ScriptFlag6;
                break;
            default:
                break;
            }

            break;
        case TagAgent:
            target.receiver = token->value;
            break;
        case TagAvoidFocus:
            ParseTaggedValueRecord(token, &avoidFocusWeight);
            target.avoidsFocus = 1;
            target.distanceOnly = 0;
            break;
        case TagNearFocus:
            ParseTaggedValueRecord(token, &nearFocusWeight);
            target.nearFocus = 1;
            target.distanceOnly = 0;
            break;
        case TagSharpTurn:
            ParseTaggedValueRecord(token, &weight);
            target.givesWeight = 1;
            target.distanceOnly = 0;
            break;
        case TagPositionCostWeight:
            ParseTaggedValueRecord(token, &positionCostWeight);
            target.positionCosts = 1;
            target.distanceOnly = 0;
            break;
        case TagAhead:
            ParseTaggedValueRecord(token, &ahead);
            options.startsAhead = 1;
            break;
        case TagStartRange:
            startRange = token->Float();
            break;
        case TagEndRange:
            endRange = token->Float();
            break;
        case TagMaxNodes:
            endFlags.kind = token->value;
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

    if (!target.takesLongJumps && !target.takesHighJumps)
    {
        target.takesJumps = 0;
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
        switch (token->tag)
        {
        case TagPointPassable:
            switches.blocked = TokenIsOn(token) ? SwitchOn : SwitchOff;
            break;
        case TagPointAirborne:
            switches.airborne = TokenIsOn(token) ? SwitchOn : SwitchOff;
            break;
        case TagPointAlwaysTaken:
            switches.alwaysTaken = TokenIsOn(token) ? SwitchOn : SwitchOff;
            break;
        case TagRange:
            unused2 = static_cast<s32>(token->value);
            switches.unused6 = 1;
            break;
        case TagHalfHeight:
            unused3 = static_cast<s32>(token->value);
            switches.unused6 = 1;
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
        if (token->tag == TagNone)
        {
            switch (token->value)
            {
            case KeywordJointAimController:
                controller.kind = NodeController::KindJointAim;
                break;
            case KeywordMaskController:
                controller.kind = NodeController::KindMask;
                break;
            case KeywordSplineController:
                controller.kind = NodeController::KindSpline;
                break;
            case KeywordSkateController:
                controller.kind = NodeController::KindSkate;
                break;
            case KeywordKeep:
                controller.keepsExisting = 1;
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
        switch (token->tag)
        {
        case TagNone:
            switch (token->value)
            {
            case KeywordSenseInstances:
                sense.bits.kind = PerceptionSense::KindInstances;
                break;
            case KeywordSenseSpeed:
                sense.bits.kind = PerceptionSense::KindSpeed;
                break;
            case KeywordSenseRising:
                sense.bits.kind = PerceptionSense::KindRising;
                break;
            case KeywordUnusedSense:
                sense.bits.kind = PerceptionSense::KindUnused;
                break;
            case KeywordGetPlayers:
                sense.bits.noticesPlayer = 1;
                break;
            default:
                break;
            }

            break;
        case TagSenseDivisor:
            sense.divisor = token->Float();
            break;
        case TagRange:
            // The radius and its square
            sense.radius = __builtin_bit_cast(f32, token->value);
            sense.falloff = token->Float() * token->Float();
            break;
        case TagSenseInterval:
            sense.interval = token->Float();
            break;
        case TagSenseDecay:
            sense.decay = __builtin_bit_cast(f32, token->value);
            break;
        case TagActor:
        {
            // A list of the objects, counted (past the eighth, into the count and on)
            u32 count = sense.objectCount;
            sense.objectCount = count + 1;
            u16* objects = sense.objects;
            objects[count] = static_cast<u16>(token->value);
            break;
        }
        case TagLowest:
            sense.lowest = __builtin_bit_cast(f32, token->value);
            break;
        case TagHighest:
            sense.highest = token->Float();
            break;
        case TagSpeedScale:
            sense.speedScale = __builtin_bit_cast(f32, token->value);
            break;
        case TagAttentionWeight:
            sense.attentionWeight = token->Float();
            break;
        default:
            break;
        }

        reader.Next();
    }
}
