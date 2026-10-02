#pragma once

#include "common.h"
#include "game/agentparts.h"
#include "game/collision.h"
#include "game/events.h"
#include "game/instances.h"

struct ChunkData;
struct ChunkLinkData;
struct TimeClock;

// An attack reaching an agent (an event of type 0x1801): its kind
struct AttackEvent : GameEvent
{
    u32 kind;
};
CHECK_OFFSET(AttackEvent, kind, 0x14);

// The agents of the object types (game/instances.h has their base). The basic agent is the base of every type's (the pickups' and
// pay gates' constructors have its construction inline) and the projectiles' own, given their vtable by the factory. What its
// functions do beyond the base's: 1 its instance's state flags applied, 9 a contact message kept and told its script, 20 an event
// of an attack (still asm), 21 its velocity given to its rigid body. Each type's agent overrides what follows its class; 11 is a
// position of its own (none but the playable characters'), 22 its frame
class BasicAgent : public Agent
{
public:
    static BasicAgent* Construct(BasicAgent* agent, InstanceCreator* creator, PropertyHolder* holder, AgentPart* part)
        RETAIL(FUN_00141318);
    // Its properties and part destroyed, then the base's destruction
    void Destroy(u32 destroyFlags) RETAIL(FUN_00141350);
    // Its instance asleep or awake and its flags (collision, visible, shadow, trigger signals) as the state says, its part made
    // again and given the state's damage and target bits
    void ApplyState(u32 unknown) RETAIL(FUN_0013de20);
    u32 Slot3() RETAIL(FUN_0013e8b0);
    void Nothing4() RETAIL(FUN_0013e8b8);
    void Nothing5() RETAIL(FUN_0013e8c0);
    // A physical contact hands the node of kind 1 the sender and the message's strength (while its instance has a physics body),
    // a message with a reaction is kept and tells its script (event 2)
    void Contact(const ContactMessage* message, InstanceContext* sender, u32 physical) RETAIL(FUN_00141480);
    void Nothing19() RETAIL(FUN_001413d8);
    // While triggers' signals reach its instance (and it can't always damage the character), an attack that reaches its part
    // tells its script (events 3 to 10 by the kind) and is recorded; one that may damage the character is recorded and makes it
    // hit back when it always may, or when the attacker's part's low byte and the attack's kind are both 3
    void Attacked(const AttackEvent* event, InstanceContext* sender) RETAIL(FUN_0013e010);
    // The instance's agent sent a damaging contact (0x400, physical) at this one's position
    void HitBack(InstanceContext* target) RETAIL(FUN_0013e1c8);
    // Its object node pushed by the other with no strength
    void Push(InstanceContext* other) RETAIL(FUN_00141538);
    // Callers pass a value and a float after the velocity, which only the playable characters' (still asm) read
    void Launch(const Vector4* velocity) RETAIL(FUN_00141588);
};
CHECK_SIZE(BasicAgent, 0x60);

class PickupAgent : public BasicAgent
{
public:
    static PickupAgent* Construct(PickupAgent* agent, InstanceCreator* creator, PropertyHolder* holder, AgentPart* part)
        RETAIL(FUN_001419b8);
    void Destroy(u32 destroyFlags) RETAIL(FUN_0013ef10);
    void ApplyState(u32 unknown) RETAIL(FUN_00141998);
    void Nothing8() RETAIL(FUN_0013ef98);
    // Every message kept and told its script, physical or not
    void Contact(const ContactMessage* message, InstanceContext* sender, u32 physical) RETAIL(FUN_001419f0);
    u32 Position(Vector4* position) RETAIL(FUN_0013efa8);
    void Frame(TimeClock* clock) RETAIL(FUN_0013efa0);
};

// The crates' (a state in bits 5-8 of its word, 2 once launched with the vertical speed after it)
class CrateAgent : public BasicAgent
{
public:
    enum Bits : u32
    {
        StateShift = 5,
        StateMask = 0xF,
        StateLaunched = 2,
    };

    u32 bits;
    f32 launchSpeed;
    u8 unknown68[0x70 - 0x68];

    static CrateAgent* Construct(CrateAgent* agent, InstanceCreator* creator, PropertyHolder* holder, AgentPart* part)
        RETAIL(FUN_001405a0);
    void Destroy(u32 destroyFlags) RETAIL(FUN_0013eb88);
    u32 Slot3() RETAIL(FUN_0013ec10);
    // Unless its part's value has bit 0 or 1, launched without speed and its script told event 11
    void Slot4() RETAIL(FUN_00140648);
    // Its script told event 11
    void Slot5() RETAIL(FUN_001406b8);
    void Nothing8() RETAIL(FUN_0013ec18);
    void Contact(const ContactMessage* message, InstanceContext* sender, u32 physical) RETAIL(FUN_001406e0);
    u32 Position(Vector4* position) RETAIL(FUN_0013ec20);
    void Launch(const Vector4* velocity) RETAIL(func_00140700);
};
CHECK_SIZE(CrateAgent, 0x70);

// The creatures' (a position, the default box's lowest corner with w 1 when made)
class CreatureAgent : public BasicAgent
{
public:
    // Bit 5 of the part's value, which the creature's functions 24 and 25 set and clear
    static constexpr u32 PartFlag20 = 0x20;

    Vector4 position;

    static CreatureAgent* Construct(CreatureAgent* agent, InstanceCreator* creator, PropertyHolder* holder, AgentPart* part)
        RETAIL(FUN_00140950);
    void Destroy(u32 destroyFlags) RETAIL(FUN_0013e8d8);
    void Nothing8() RETAIL(FUN_0013e8f8);
    // A physical contact pushes it (no strength) while it has a physics body, every message is kept and tells its script
    // unless it can always damage the character
    void Contact(const ContactMessage* message, InstanceContext* sender, u32 physical) RETAIL(FUN_00140ae0);
    // Only through links with bit 18, its position taken into the linked chunk's space
    u32 CanChangeChunk(ChunkData* from, ChunkLinkData* link) RETAIL(FUN_00140ca0);
    u32 Position(Vector4* position) RETAIL(FUN_0013e908);
    void Attacked(const AttackEvent* event, InstanceContext* sender) RETAIL(FUN_00140ac0);
    // Its function 25 when its part's flags have bit 2
    void Slot23() RETAIL(FUN_00140908);
    void SetPartFlag() RETAIL(FUN_001409b0);
    void ClearPartFlag() RETAIL(FUN_001409c8);
};
CHECK_OFFSET(CreatureAgent, position, 0x60);
CHECK_SIZE(CreatureAgent, 0x70);

// Eight words the playable characters' agent keeps 0xF0 bytes in, zeroed when made
struct CharacterWords
{
    u32 words[8];

    void Clear() RETAIL(FUN_0013e910);
};
CHECK_SIZE(CharacterWords, 0x20);

// The references a playable character's agent keeps 0x110 bytes in (still asm): 32 handles after their count
struct CharacterReferences
{
    u32 count;
    Reference* handles[32];
    u8 unknown84[0xF0 - 0x84];

    // Made empty, a reference to an object taken when there's one
    static CharacterReferences* Construct(CharacterReferences* references, ReferencedObject* first) RETAIL(FUN_001f4bc0);
    void Destroy(u32 destroyFlags) RETAIL(FUN_001f5e58);
};
CHECK_SIZE(CharacterReferences, 0xF0);

// The playable characters' (still mostly asm): the controllers its setup makes by the character (its first integer property),
// the words at 0xF0, its references, three handles and its collision cache
class CharacterAgent : public CreatureAgent
{
public:
    u8 unknown70[0x98 - 0x70];
    void* controllers[11];
    u8 unknownC4[0xF0 - 0xC4];
    CharacterWords words;
    CharacterReferences references;
    Reference* handle200;
    u8 unknown204[0x290 - 0x204];
    Reference* handle290;
    u8 unknown294[4];
    Reference* handle298;
    u8 unknown29C[0x2B0 - 0x29C];
    CollisionCache cache;
    u8 unknownEnd[0x310 - 0x2B0 - sizeof(CollisionCache)];

    static CharacterAgent* Construct(CharacterAgent* agent, InstanceCreator* creator, PropertyHolder* holder, AgentPart* part)
        RETAIL(FUN_00136f70);
    void Destroy(u32 destroyFlags) RETAIL(FUN_0013f920);
    // Its position (the creature's) is its own
    u32 Position(Vector4* position) RETAIL(FUN_0013e998);
    u32 Slot12() RETAIL(FUN_0013e938);
    // Its center is its model's joint 1 (not in the world)
    void CollisionCenter(Vector4* center) RETAIL(FUN_0013fd98);
};
CHECK_OFFSET(CharacterAgent, controllers, 0x98);
CHECK_OFFSET(CharacterAgent, words, 0xF0);
CHECK_OFFSET(CharacterAgent, references, 0x110);
CHECK_OFFSET(CharacterAgent, handle200, 0x200);
CHECK_OFFSET(CharacterAgent, handle298, 0x298);
CHECK_OFFSET(CharacterAgent, cache, 0x2B0);
CHECK_SIZE(CharacterAgent, 0x310);

class GenericObjectAgent : public BasicAgent
{
public:
    static GenericObjectAgent* Construct(GenericObjectAgent* agent, InstanceCreator* creator, PropertyHolder* holder,
                                         AgentPart* part) RETAIL(FUN_00141060);
    void Destroy(u32 destroyFlags) RETAIL(FUN_0013ed40);
    // A collision tells its script (event 3, the other its sender) when attacks of kind 3 reach it: whether it did
    u32 Collided(void* other, const Vector4* point, const Vector4* impulse) RETAIL(FUN_001410c0);
    void Nothing8() RETAIL(FUN_0013edc8);
    u32 Position(Vector4* position) RETAIL(FUN_0013edd0);
    void Attacked(const AttackEvent* event, InstanceContext* sender) RETAIL(FUN_001410a0);
    void Frame(TimeClock* clock) RETAIL(FUN_00141098);
};

class GrabbableAgent : public BasicAgent
{
public:
    static GrabbableAgent* Construct(GrabbableAgent* agent, InstanceCreator* creator, PropertyHolder* holder, AgentPart* part)
        RETAIL(FUN_00140338);
    void Destroy(u32 destroyFlags) RETAIL(FUN_0013ea50);
    u32 Position(Vector4* position) RETAIL(FUN_0013ead8);
    void Frame(TimeClock* clock) RETAIL(FUN_00140370);
};

class PayGateAgent : public BasicAgent
{
public:
    static PayGateAgent* Construct(PayGateAgent* agent, InstanceCreator* creator, PropertyHolder* holder, AgentPart* part)
        RETAIL(FUN_00141800);
    void Destroy(u32 destroyFlags) RETAIL(FUN_00141bf8);
    u32 Position(Vector4* position) RETAIL(FUN_00141c20);
    void Frame(TimeClock* clock) RETAIL(FUN_00141c18);
};

// The graples' (an object of its own, destroyed with it; its frame runs while its instance is visible)
class GrapleAgent : public BasicAgent
{
public:
    struct Owned
    {
        const GccVTableEntry* vtable;
    };

    Owned* owned;
    u8 unknown64[0x90 - 0x64];

    static GrapleAgent* Construct(GrapleAgent* agent, InstanceCreator* creator, PropertyHolder* holder, AgentPart* part)
        RETAIL(FUN_00140e78);
    void Destroy(u32 destroyFlags) RETAIL(FUN_00140eb0);
    // Its object made the first time instead (its state isn't applied)
    void ApplyState(u32 unknown) RETAIL(FUN_00140e20);
    u32 Position(Vector4* position) RETAIL(FUN_0013ecb0);
    void Frame(TimeClock* clock) RETAIL(FUN_00140f30);
};
CHECK_SIZE(GrapleAgent, 0x90);

// The projectiles' (the basic agent given its vtable by the factory): its instance flagged 0x20000 when its state is applied
class ProjectileAgent : public BasicAgent
{
public:
    static constexpr u32 InstanceFlag = 0x20000;

    void Destroy(u32 destroyFlags) RETAIL(FUN_0013f090);
    void ApplyState(u32 unknown) RETAIL(FUN_00141b98);
    void Nothing8() RETAIL(FUN_0013f120);
    void Contact(const ContactMessage* message, InstanceContext* sender, u32 physical) RETAIL(FUN_0013f128);
    u32 Position(Vector4* position) RETAIL(FUN_0013f138);
    void Frame(TimeClock* clock) RETAIL(FUN_0013f130);
};

extern "C"
{
    extern const GccVTableEntry g_AgentVTable[] RETAIL(ObjectInstanceContextBase_Methods);
    extern const GccVTableEntry g_BasicAgentVTable[] RETAIL(D_002F2BA8);
    extern const GccVTableEntry g_PickupAgentVTable[] RETAIL(D_002F2280);
    extern const GccVTableEntry g_CrateAgentVTable[] RETAIL(D_002F2700);
    extern const GccVTableEntry g_CreatureAgentVTable[] RETAIL(D_002F2AD0);
    extern const GccVTableEntry g_CharacterAgentVTable[] RETAIL(PlayableCharacterObjectInstanceContext_Methods);
    extern const GccVTableEntry g_GenericObjectAgentVTable[] RETAIL(D_002F2460);
    extern const GccVTableEntry g_GrabbableAgentVTable[] RETAIL(D_002F2850);
    extern const GccVTableEntry g_PayGateAgentVTable[] RETAIL(D_002F2EB0);
    extern const GccVTableEntry g_GrapleAgentVTable[] RETAIL(ObjectInstanceContextType7_Methods);
    extern const GccVTableEntry g_ProjectileAgentVTable[] RETAIL(ObjectInstanceContextType8_Methods);

    // The playable character's controllers made by the character it is, and let go of (still asm)
    void SetUpCharacter(CharacterAgent* agent, u32 made) RETAIL(FUN_00134e88);
    void TearDownCharacter(CharacterAgent* agent, u32 unknown) RETAIL(FUN_00135110);
    // The graple's frame while its instance is visible, its object (0x120 bytes) made for its instance with a value (1), and a
    // value set in it (still asm)
    void GrapleFrame(GrapleAgent* agent, TimeClock* clock) RETAIL(FUN_0013d818);
    GrapleAgent::Owned* ConstructGrapleOwned(f32 value, void* memory, InstanceContext* instance) RETAIL_N32(FUN_00162098);
    void SetGrapleOwnedValue(GrapleAgent::Owned* owned, u32 value) RETAIL(FUN_00191a38);
}
