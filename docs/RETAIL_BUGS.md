# Bugs in the retail code

Crash Twinsanity PAL (SLES_525.68).

## How this was verified

The C++ keeps every one of these as retail has it, with a comment where it happens: this is the list, not a fix list.

- Every candidate the conversion turned up was read again in the retail asm (`asm/text`), not in the C++. Each entry gives the
  instruction address of the faulty spot.
- **Reached in normal play** comes from the retail data where it could be checked: the 1,846 behaviour scripts of a TT Lab project
  made from the PAL disc (commands and conditions named by TT Lab's AgentLab definitions), the trigger and camera files and the
  language files. "Unknown" means the data or the state at run time decides it.
- **Classes:**
  - **C** (confirmed): the asm does what's described.
  - **P** (plausible): the asm does it, but whether it's a bug depends on what was meant.
  - **latent**: real, but nothing in the game's data reaches it.
  - **dead**: the code never runs.
- **Low addresses:** the game runs with kernel RAM mapped at 0. A read through a null pointer returns kernel memory (garbage,
  no crash). A write there corrupts the kernel's memory. A virtual call or jump through such a pointer crashes.
- **(conv.)**: minor items taken from the conversion's notes and the C++ without reading their asm again.

## The game's own bugs

### AI and scripts (commands, conditions, packets, attachments)

- **C, reached:** Cmd48 DropAttachedObject (0x24DB90).
  - Bug: modes 1 and 3 launch the dropped object with a velocity at sp+0 that nothing writes. The detach functions
    FUN_00196d88 and FUN_00196e00 ignore the pointer they're given, and the launches happen at 0x24DD4C and 0x24DDAC.
  - Effect: the object flies off with stack leftovers.
  - Reached: yes. Mode 1 is used by COM_ANT_SML_ECOLOGY_DEFAULT and COM_ANT_SML_ECOLOGY_COLLECTOR (Ant Agony's small ants).
- **C, reach unknown:** Cmd146 SetFocusToLinkedObject (0x2150D0).
  - Bug: "last linked" is (count-1)&0xFF (0x2151E8-0x2151F0). With no links it reads slot 255 (attachments+0x41C) and takes
    that word as an instance. "Current linked" can reach index 31 of 16 slots.
  - Effect: a garbage instance becomes the focus.
  - Reached: 3 retail uses of lastLink; unknown whether the list can be empty at that moment.
- **C, reached only when something is missing:**
  - Cmd177 WarpAgent (0x217300): a subject with no instance leaves s2 = 0, and the place is read at 0x8(s2) (0x217568).
    Effect: it warps through kernel memory (corruption). 17 retail uses, all with focus/AgentRef1 subjects.
  - Cmd169 SetFocusPositionAlong (0x215460): start sp+0x10 and end sp+0x20 are only written by the instance path or the
    designator-position call (0x2154B4, 0x2155B8). Effect: stack garbage. 7 retail uses.
  - Cmd121 SetFocusPositionBesidePlayer (0x214188) and Cmd60 (conv.): with no player they read its place at address 8.
- **C, reach unknown:** Cmd8 SpawnResidentAgent (0x21F830).
  - Bug: a spawned instance with id -1 gives v0 = 0 (0x21FE5C), and the spawner is stored at address 4 (0x21FE64).
  - Effect: a kernel memory write.
  - Reached: only if a spawned instance gets no ID.
- **C, reach unknown:** CheckPacketEnd FUN_0020eab8.
  - Bug: without a focus instance it walks the node list of whatever the caller left in a3 (a0 = a3+0xD4 at 0x20EB40). The
    only caller, FUN_0020e258, sets a3 = s0+0x78 on one path (0x20E444).
  - Effect: a wrong packet end, or a walk through a garbage pointer.
- **C, reach unknown:** StartPacketMotion FUN_0020cd10.
  - Bug: for a packet that turns a joint, the rotator's start is copied from sp+0..0xC (0x20DC10-0x20DC34). That slot is only
    written on the no-joint path (0x20DC7C).
  - Effect: joint turns start from a garbage rotation.
- **C, reach unknown:** Cmd10 DoParticle ExecuteOn (0x21AA00).
  - Bug: a waypoint key index at or past the key count gives v0 = 0 (0x21AA54/0x21AA68), then `lq 0($v0)` (0x21AA7C).
  - Effect: the particle is placed at a position read from address 0.
  - P: the surface mode only compares its byte with 0xFF (0x21AAD8). It then plays the particle system whose index is the
    node's water-surface or surface index (0x21AB00/0x21AB18).
- **P, reached:** Cond141-146 (0x2421A0-0x242DC8).
  - Bug: they find the point on the instance's axis line nearest the target, then measure from the instance to that point
    (141: 0x2423F4-0x24241C, 145: 0x242D70-0x242DA4). That's the distance along the axis, not off it. The Y-axis
    "HorizontalDistance" ones give the vertical distance.
  - Reached: 145 (3 uses) and 146 (5 uses); 141-144 unused.
- **C, latent:** Cond128 AngleToFocus (0x228D50). A mode other than 0 or 1 jumps on with an uninitialized target (0x228D80).
  The builders only make 0 and 1.
- **P, not reached:** Cond526/527/528 CanMoveBackwards / CanStrafeLeft / CanStrafeRight. They pass when the way is blocked
  (beqz at 0x1280DC, 0x128274, 0x128408); Cond525 passes when it's free (bnez at 0x127F40). No retail script uses 525-528.
- **C, not reached (matters for modded scripts):** BuildScriptCommand (0x101720).
  - Bug: the command table's entries 18 and 19 both build a NowTurnLeft (vt_Cmd18_NowTurnLeft at 0x1031C0 and 0x1031DC).
  - Effect: NowTurnRight turns left. Its own Execute FUN_00215fd0 sits in a vtable nothing uses.
  - Reached: no retail script uses either command.
- **C, not reached (every retail use avoids the faulty mode):**
  - Cmd23 NowRotateJoint (0x24CA00): mode 0 without a focus links the caller's $s0 (the command object) as an instance
    (0x24CA48/0x24CA54, then FUN_00196258 at 0x24CB10). All 3 retail uses are mode 2.
  - Cmd51 UnsupportAbove (0x24F160): the "nearest" search starts at 1e30 (0x24F290) and keeps only farther instances
    (0x24F394), so it never finds one. 0 uses.
  - Cmd149 RotateWithLinked (0x24F5A0): with none of axis bits 3-5 it turns about an uninitialized axis (read at 0x24F66C).
    All 8 retail uses set one.
  - Cmd54 SendUserMessage (0x24FFA0): the by-index byte is unbounded (0x250830-0x25083C). An empty "last" writes 0xFF into
    the command itself (0x250808). All 599 retail uses send by object or designator.
  - Cmd70 ContinueColliderMotion (0x244710): the rigid body isn't checked (0x244740) and bit 0 turns an unset stack vector
    into the velocity (0x244778). 0 uses.
  - Cmd72 ColliderLaunchNow (0x244CC8) and Cmd193 LaunchAtTarget (0x2452F0): with neither a velocity nor a throw flag the
    velocity is unset (193: 0x2454B8). 193 also reads gravity at 0xA4 without a body (0x245438). All 110 + 15 retail uses
    set a flag.
  - Cmd45 SetFocusToAgent: designators other than 0xDE/DF/F7/F8/FB are taken as receiver indexes. Retail uses 0-2, 0xDE,
    0xDF and 0xF7.
  - Cmd525 RaycastFocusPosition: with a distance it scales the absolute point toward the world origin (0x112F54-0x112F94).
    All 11 uses have distance 0.
  - Cmd549 GetShortRoute: DistanceOnly + NearFocus hands over a stack value (conv.). No retail use sets both.
  - Cmd523 ApplyVelocity FUN_0010c570: a target point level with the agent divides by a time of 0; one below takes the root
    of a negative (conv.). Every retail point is above.
  - Cmd150 StrafeTowardsTarget: its distance limit reads an unset start (all 5 uses have no limit). Cmd8's angles are unset
    for spaces other than 0-2 (only 1 and 2 are used). Cmd152 AddMotionAngles shrinks the phase by 57.3 each use, and Cmd200,
    Cmd138 and Cmd153 have their own slips. These four have 0 uses (conv.).
  - Cmd655/Cmd661 Set Mask/Skate ControllerIds (0x121CE8/0x119EC0): their counts are never reset, but they only append
    again with the add-again bit 0x200, which no retail use sets. COM_CORTEX_START_SKATE_STUB re-runs Cmd661 without it.
  - Cmd534: negative areas are kept as their low 5 bits (retail uses 3-31). Cmd658: its x and y offsets are dropped (both
    retail uses pass 0).
  - Cmd551/Cmd552 Open/CloseAllLinkedFurniture both send event 1 (0x129014, 0x1290C4). P, 0 uses.
- **C, reach unknown:** Cmd49 ThrowAttachedObject (0x24E4A0).
  - Bug: it clears the "holds others" flag 0x80 (0x24E528-0x24E534) even when other objects stay attached.
  - Effect: the instance can no longer drop or throw them.
  - Reached: 1 retail use.
- **C, latent:** the joint aim controller (node controller kind 0).
  - Bug: Stop FUN_0011dee8 is an empty `jr ra`, and the destructor FUN_0011de78 frees it without the unhook (FUN_001217f0)
    that Restart FUN_0011dec8 does.
  - Effect: a controller replaced while its model lives leaves the animator posing through a freed hook (use after free).
  - Reached: only COM_FINAL_BOSS_ACTIVATED makes one. Cmd641 FinalBossInitWeapons (0x116A78) doesn't check the controller's
    kind, but that script has made kind 0 first.
- **C, reach unknown:** RouteStepPosition FUN_0020b6e8.
  - Bug: the push direction's y is the point's height (lwc1 0x4($sp) at 0x20BB44, stored at 0x20BB54) instead of y times
    the inverse length.
  - Effect: a push toward the instance lifts the route point by its height x room x push.
- **C, latent:** TriggerNode::AddInstance FUN_001f6298.
  - Bug: the instance count (byte 0x19) is never capped at the 35 slots from 0xE4.
  - Effect: more instances overwrite what follows.
  - Reached: RegisterTrigger and RegisterCamera add each trigger's or camera's listed instances at load; the retail maximum
    is 10 (of 1,130 PS2 triggers and cameras).
- **C, hang, reach unknown:**
  - DetachAllSprings FUN_00197140 (Cmd81): a non-spring attachment branches to the loop's end without moving the index
    (0x1971A8 -> 0x197250), so the loop never ends. 2 retail uses (COM_PSYCHOTRON_BEAM_DEACTIVATED,
    COM_TIKI_MON_LASER_TARGET_DEACTIVATED); it only hangs if that instance also holds something else.
  - ReleaseAttachmentsNode FUN_00196ec8: an instance attachment with neither a reference nor an instance runs UnlinkMissing
    (0x197030) and takes the same index again forever. HoldInPlace FUN_00196958 makes exactly such an attachment (Add(held,
    null), a2 = 0 at 0x19695C). Releasing a node that holds one hangs.
  - (conv.) UnlinkSpawned 0x197348 and ReleaseLinkedInstances 0x197618 loop forever on an empty or kept link.
- **C, reach unknown:** PickupObjectNode::SetState FUN_00108e68.
  - Bug: fleeing (state 8) without an awake focus calls SetState(idle) (0x109234) and then still stores state 8 (0x109240+).
    The return is missing.
  - Effect: the pickup flees with its old velocity, and its previous state reads idle.
- **(conv.) smaller ones:**
  - AttachToAiPosition 0x196798 leaks its 0xC0-byte attachment.
  - HangOnExitPoint 0x196408 and AttachInstance 0x1969B0 launch with a stack velocity.
  - DesignatedPosition FUN_0022d9e8 and SpaceOfRequest FUN_0022e0b8 read address 8 without a player.
  - HitWhileMoving FUN_0023dcb0 doesn't check its physics body.
  - DestroyHeadTurner FUN_0023fbc8 doesn't release 3 references (leak).
  - StepHeadTracking FUN_00237448 turns toward the old target on the step it switches back.
  - SnapInstanceRotationX/Y/Z truncate toward 0. Their only user, Cmd113, has 0 uses.
  - Trajectory code: an instance missing from its parent's path is read and written at address 0 (StartFollowing
    FUN_00238370, TrajectoryFrame FUN_0023ae68, HoldTrajectory FUN_0023b8d0). StepTrajectory FUN_00238ed8 adds space 7's
    move to the previous one (it grows every frame), and space 4's moves the instance each frame. StepCoverSearch
    FUN_0023a600 reads address 8 without a player.
  - Many command null-check gaps: Cmd625 calls the agent's slot 12 before its null check, plus Cmd521, Cmd555, Cmd617,
    Cmd519, Cmd600, Cmd578/579, Cmd178 and CreateCrateContents.
  - Cmd131 MagnetPullToFocus: verified at 0x245CF4-0x245D04. It calls through the vtable of an unchecked GetGameNode result,
    which crashes if an attached focus's parent has no object node (1 retail use).

### Characters

- **P:** FUN_00178290 clears flag 0x80000 on characters 0-3 and 5 but skips character 4. Possibly intended.
- **C, reached:** WalkController::AskTurn FUN_0014f330.
  - Bug: in the 45-135 degree band the turn is eased by |(wanted-45)/90| for both signs (0x14F508-0x14F578).
  - Effect: turns one way are scaled 0.5-1, the other way 1-1.5, before the per-frame clamp. Turning is lopsided.
  - Reached: walking.
- **C, reached:** JumpController::Rise FUN_001491e0.
  - Bug: when attack isn't pressed it clears the knee drop's wait-for-room bit 25 (0x14930C-0x14931C), then tests that bit
    (0x149350-0x14935C).
  - Effect: the knee drop never waits or retries.
- **P, reached:** JumpController FallFrame FUN_001495b0.
  - Bug: it sets gravity to float property 1 every frame (0x14973C-0x149744), after Fall set the jump kind's own falling
    gravity.
  - Effect: the per-kind gravities last one frame. Every fall.
- **C, reached:** WalkIntoBody FUN_001332d0.
  - Bug: for a sphere body it works out the unit sideways direction, then adds a vector of f20 = 0 (0x1334AC;
    0x133724-0x13375C).
  - Effect: walking into a physics sphere never pushes it sideways.
- **C, reached:** KeepPushedBody FUN_00133c58.
  - Bug: it normalizes the way to the body (0x133DAC-0x133DC4), then tests that length < 4 (0x133E20-0x133E40).
  - Effect: the test always passes, so a pushed body is kept at any distance ahead.
- **C, reached:** LookController::Construct FUN_00147310.
  - Bug: neither it nor FUN_00147380 stores restSeconds (0x60), and the heap isn't cleared.
  - Effect: a new head-look rests for the heap's leftover float. A NaN would rest forever.
  - Reached: every character's look; how visible is unknown.
- **P, reached:** ProceduralJoints::PoseJoint FUN_0014b7d0, the squash pose (Crash's joint 28 on landing).
  - Bug: it computes 2 - squash and compares it with 1 (0x14BD2C/0x14BD30), then writes 1 to x and y (0x14BD50/0x14BD58) and
    the squash to z.
  - Effect: the landing squash has no sideways bulge.
- **P, reached:** CharacterAgent::Solve FUN_00139678.
  - Bug: the crush test is pushed - |push|^2 > 0.25 (0x139BD4-0x139C0C); SolveLinked's swing test uses the length.
  - Effect: with pushes under a unit the character is crushed by shallower penetrations.
- **C, unlikely in play:** CharacterAgent::Contact FUN_00137510.
  - Bug: for a driving vehicle (bit 1) the vehicle's other agent becomes the passenger with no type check (0x1375C0/0x1375CC).
    The hoverboard (kind 5) and the wrestle (kind 6) set bit 1, and their other agent is the board or creature.
  - Effect: when the rider is hurt, that agent gets the contact, hit points in its part flags, KnockBack, or Die (writes past a
    creature agent's end). Memory corruption.
  - Reached: the hoverboard is only made by COM_ACTIVATE_HOVERBOARD on a cutscene actor (Bossarea H01D), the wrestle only by
    COM_CRASH_WRESTLE_CREATURE in the leftover CavEnt_old chunk.
- **C, latent:** LeaveVehicle FUN_0013bc60. The null check's beqz (0x13BCFC) jumps onto the store itself, so `sw $zero,
  0x18($s0)` (0x13BD0C) runs with s0 = null. Address 0x18 is written when the passenger has no controls node.
- **P:**
  - JumpFrame FUN_0012e638: without a jump controller it clears part bits 33, 34, 39 and 40 (0x12E66C-0x12E6B8) but not 35
    (the slide jump).
  - Gun::Frame FUN_0014a100: drawing and holstering wait the last shot's duration (0 after a reset).
  - TakeAmmo FUN_0015ef58: a shot costing more than is left empties the ammo.
  - MeasureVelocity FUN_0012e328 fetches the ridden instance's move and drops it (conv.).
- **(conv.) smaller ones, mostly latent:**
  - SolveLinked doesn't reset the ground normal; a doubly stuck move falls back to contact 0.
  - Swinging over a sphere uses prefilled y 0.001.
  - ClingToWall 0x135D90 makes the wall-cling vehicle again for every qualifying wall.
  - PlaceAt and ChangeHeight return QueueObject's leftover v0.
  - Null reads at 0x80, 0x44, 0xB0 and 0xB4 in RideMove, KeepStanding, Probe, Unlink FUN_00132dc0 and the second
    character's SpinController Start/End 0x14E950/0x14EA60.
  - CheckTriggers 0x131290 loops forever on an empty handle (never produced).
  - HullHits 0x136AE0 has room for 24 hulls and never checks.
  - CrouchController runs event 54 twice when slowing below 0.2.
  - Claw: Retract (state 3) is never entered. Fly's end for other grab kinds has no next state. WindUp turns toward last
    frame's point. PlaceGraple 0x144A90 and CanTarget 0x15C8E8 read agents unchecked. FUN_001467d0 reads past 16 springs.
    TargetLock's blockingKinds are never set.
  - GameFactoryStandIn FUN_0012dee0: character property 6 writes progress.characters[6] = checkpoints[0].
  - PoseJoint reads one element past the end when no element has the joint (can't happen).

### Vehicles (Rollerbrawl and Humiliskate are in normal play)

- **C, reached:** HumiliskateVehicle::TouchesHull FUN_001578c0.
  - Bug: it multiplies the whole contact point by -rx, -ry and -rz in turn (0x1579F8-0x157A50), so every component gets all
    three radii.
  - Effect: the touched instance gets a wrong contact point.
- **C, reached:** LaySkidMark FUN_0015bd20.
  - Bug: it branches twice on the near end's height test (bc1t at 0x15C030 and 0x15C038); the far end is never tested.
  - Effect: strips get laid with their far end deep under the surface.
- **C, reached:** ConstructSkidMarks FUN_00161ac0 never sets lastPoint (0x10) or the two strip colours (0x30/0x34).
  - Effect: the first strip of each trail is coloured from heap leftovers, and its length is measured from a garbage point.
- **C, unlikely in play:** WrestleVehicle::SetVelocity FUN_00161368.
  - Bug: after stopping the body it calls ApplyImpulse with the zero vector as the impulse (a1 = s0) and the 32 u/s velocity
    as the point (a2 = sp) (0x161400/0x161408). The arguments are swapped.
  - Effect: the ball always starts still. Only in CavEnt_old.
- **P:** WallClingVehicle::SetVelocity FUN_001606e0 is an empty `jr ra`, so the cling ignores the velocity it's given.
- **P:** FollowRide FUN_00142820 keeps the vehicle kind in 3 bits (andi 7 at 0x142984). A passenger (kind 8) never matches,
  so SetTilts(1) runs every frame while tilting.
- **(conv.):**
  - Humiliskate FindRails 0x158160 and Collide 0x155978 never check their 32 rail triangles, 30 rails and 256 cached
    triangles (stack overflow with dense geometry). Passing a rail's end restarts the substep.
  - CollideInstances and Rollerbrawl HitInstances overflow with more than 19 attached instances.
  - SendEvents, Tricks and Frame don't check the other agent or the object node.
  - Rollerbrawl GrowSnow/WearSnow take the mass from the uncapped snow. StoppedFrame/SquashedFrame call Place twice.
  - The hoverboard's stick-as-cross is overwritten by ReadButtons (VehicleControls::Frame 0x162358).
  - WallClingVehicle::Push 0x160AF8 doesn't check its instance.

### Physics and collision

- **C, latent:** CastHullDown FUN_002816f8 (0x2816F8). A hull of zero height keeps the step at 0, so the loop never ends
  unless the hull starts inside something. Every hull in the game's data has height.
- **C, reached:** OverlappingRangesSpan FUN_0018cdb8.
  - Bug: it returns |min(lows) - max(highs)| (0x18CDD8-0x18CDE4), the union of the two ranges, not their overlap.
  - Effect on CellHoldsBox FUN_001fa4b0 (call at 0x1FA53C): any overlap counts as wholly held (1), never 2. UpdateCollisionCell
    FUN_001eb250 refiles only on != 1 (0x1EB2C0), so an instance stays filed in its old scenery cell until it stops touching
    it. The root check lets boxes sticking out of the root count as inside.
  - Reached: every moving instance. The visible effect (cell queries or culling missing straddling instances) is unknown.
- **C, crash, reach unknown:** UpdateCollisionCell FUN_001eb250.
  - Bug: in a chunk without a scenery root it builds and drops the object's name (the leftover of a message), then reads a
    vtable at 0x44(null) (0x1EB340) and calls through it (0x1EB354).
- **C, reached:** CollideRigidBodies FUN_002498c8.
  - Bug: it scales the push by depth*0.5/distance (0x249A38-0x249A4C), then again by +-depth/distance
    (0x249A60-0x249A84, 0x249BC0-0x249BD0) or by the half factor again (0x249CF8-0x249D04). CollideUprightBodies FUN_0024a078
    does the same (conv.).
  - Effect: overlapping bodies separate by depth^2/(2d) or depth^2/(4d) a frame instead of depth or depth/2, so overlaps take
    several frames to resolve.
- **C, reached:** StepMovement FUN_00230e58.
  - Bug: it clears bits 51-53 of the rigid body (masks 0x230ED4-0x230F30, store at 0x230FEC) just before calling
    ReleaseRigidBodyAtRest FUN_0023df20 (0x231010), which tests bit 52 (0x23DF30).
  - Effect: resting bodies are never released (extra work).
- **C:** SlideAgainstWorld FUN_00249618. The knock is before - velocity with the velocity unchanged (0x249868), so it's always
  0 and a slide never knocks the node.
- **P, reached:** MakePhysicsBody FUN_00247270.
  - Bug: for hull kinds 10/11 it computes 2*(box max - min) into sp (0x24736C-0x2473D4) and never uses it. Only spheres get
    SetMassAndSize (0x24734C).
  - Effect: hull bodies keep the default mass and size.
  - Reached: COM_CAVERN_LIFT_DEFAULT (kind 10, no size given).
- **P:** SetCollisions Cmd44. A motion kind given for an existing body is skipped (bnel at 0x244100) while the collision kind
  is applied (0x24416C). No retail script changes a kind within itself.
- **C, reach unknown:** SetRigidBodyLengthDrag FUN_00254310.
  - Bug: it gives the physics body (0x320) the rigid body's drag (0xA8) instead of the length drag it just stored (0x254330).
  - Reached: no retail SetCollisions gives a length drag; the trajectory code calls it too.
- **C, not reached:** Activate/Deactivate rigid body (FUN_00246950 / FUN_00254088).
  - Bug: deactivate forgets the first-list index (0x2540DC) but leaves the body in the list. Activate re-adds it there
    (0x2469CC-0x246A08) and re-adds to the second list only while it's still indexed (0x246A28).
  - Effect: duplicates and dangling pointers.
  - Reached: only Cmd128 PhysicsBodyReset deactivates (0 retail uses), and only it sets the out-of-action bit.
- **C, harmless:**
  - The 22/23-plane copies in AddTriangleContact FUN_00284da8 (0x284EC0-0x284EE8) and FUN_00288430 (0x288490-0x2884B8)
    allocate and copy 24 planes, reading up to 32 bytes past a source sized 2 x axes.
  - LineBoxTwoZeros FUN_001f8100: below -e[i2] the store goes to p[i1] (0x1F820C). Eberly's original has p[i2]. The caller
    ignores the point.
  - GetBbox 0x201BF0 transforms the first point before checking the count.
- **C, reach unknown:** SegmentInBox FUN_001f70f8 (box virtual slot 6). It sets result = 2 (0x1F7220) before the face tests, and
  a start inside leaves 0 (0x1F720C). A segment starting outside always "crosses"; one going from inside out is "apart".
- **C, latent:**
  - BoxHullOf FUN_001fdc30 has no room check: a 9th box (0x1FDD50/0x1FDD64) overwrites the hull pointers and the count. Only
    3 fixed boxes are ever asked for.
  - DoDynamicSceneryAnimation 0x1FE640 subtracts frames x seconds while frames <= at, so a 0-frame model loops forever.
- **C, dead:**
  - Capsule and sphere volumes: the capsule constructor FUN_001f8390 is only called by its own copy, and their methods have no
    callers.
  - Their bugs never run: radius = the moved point's w, an uninitialized radius and half length, a radius-0 Contact loop.
  - SegmentsDistanceSquared FUN_00185088 (for example 3*b0 + tmp*t at 0x1853A0-0x1853B0) is only used by those capsules.
- **P (conv.):** ConstructRigidBody leaves bits 48, 49 and 53 of 0x88 and 1, 2 and 26-31 of 0x90 as the heap had them; Falls is
  set right after by SetCollisions. RideMovement 0x2490A8 multiplies the ride's move by the frame time. StepRigidBody's
  fall-only step takes x/z from the grip it just zeroed.

### Cameras

- **(conv.), harmless:** unchecked camera pointers. FUN_00172670 writes the camera's clock index without checking the camera
  exists (byte 0x153 with none); FUN_001728b8 and FUN_0017b4b8 read the camera instance without a null check; FUN_001759e0
  (the cutscene camera toggle) clears flag 0x20000 of camera 0 unchecked, and for camera 3 with no player reads its place
  from address 8. The game always has its camera.
- **C, reached:** RollerbrawlYaw FUN_00142af8.
  - Bug: above 50 u/s it replaces the speed by the most yaw speed in radians, D_0030A5A8 x 2pi/65536 ~ 3.49
    (0x142B1C-0x142B38).
  - Effect: the camera swings about 37 deg/s instead of 200 when the ball is fastest.
- **C, visibility unknown:** MoveAim FUN_00110878, used by Cmd591 CutsceneCameraMove.
  - Bug: aim mode 2 ("both") is (second - first) * 0.5 (0x1109D0-0x110A38): half the offset, not the midpoint.
  - Reached: 19 of 388 retail uses.
  - Same family:
    - In the path branch the aim at sp+0x20 (0x110380) is unset for modes 3-7 (1 retail use of mode 5). The non-path branch
      initializes it (0x1104A8).
    - DistanceFromAim's mode 2 has the same slip but no use.
    - With no second object, DistanceFromAim uses the first place for the second (conv.).
- **C, reach unknown:** FollowCameraPositioner::FindClearPlace FUN_00276910.
  - Bug: the second fallback compares with longPull (f22) and takes farSquared (f23) (0x276E34/0x276E6C). The other two
    placements do the opposite (0x276A1C, 0x276C28).
  - Effect: the camera is pulled back by radius^2+4 where radius+2.1 was meant.
- **C, not reached:** FollowCameraPositioner::Step FUN_00274048. While holding still it hands MoveToward a never-written stack
  place (sp+0x30, 0x2747D0/0x2747D4) as the goal. None of the 452 PS2 cameras sets flag bit 21 (HoldsStill).
- **(conv.):** SetTilts 0x1425D8 writes the yaw hold twice. StepFollowCamera's bit 3 is never set. FollowCameraTakes 0x1434A0
  returns byte ^ 1. VehicleControls::Frame 0x162358 reads address 8 without a camera object.

### Rendering

- **C, reach unknown:** blend-skin materials, FUN_001bbc08 (called from the blend-skin draw).
  - Bug: every shader layer gets shaders[0]'s texture (`lw 0x0($s1)` at 0x1BBD98, s1 never advanced). WriteShaders uses each
    shader's own texture.
  - Effect: a multi-layer blend-skin material draws every layer with the first texture.
- **P, reached:** ClipSegmentToPlanes FUN_001fdf90.
  - Bug: it moves an end that's outside a plane along the unit direction by its plane distance (0x1FE154-0x1FE160,
    0x1FE1E8-0x1FE1F4), with no division by the cosine.
  - Effect: the end stays outside unless the segment is square to the plane. It clips every character shadow segment
    (ChunkShadows::Draw FUN_001ca788); minor.
- **C, not reached:** BlendPalettes FUN_001aecc0. Its source pointers never advance (0x1AED10/0x1AED18), only the destination
  (0x1AEE10), so every entry is a blend of the two palettes' first colours. The blend flag D_00309C32 starts at 0 and is only
  ever written 0.
- **C, not reached:** ShadowShapes AddCircle FUN_001cbf30 and AddCapsule FUN_001cbf58. They build the u64 joint mask with a
  32-bit sllv (0x1CBF40, 0x1CBF6C/0x1CBF74), so joint 31 sets bits 31-63 and joints 32+ wrap. Only Cmd188/189 add shapes, and
  no retail script uses them.
- **C, latent:**
  - AnimateShader 0x297260 subtracts the loop length while time >= length (0x2972B8-0x2972D8). A shader animation of 0 frames
    hangs on its second frame.
  - NextPowerOfTwo FUN_002c6d70 uses a signed compare, so an input above 2^30 loops forever (0x2C6D88-0x2C6D9C).
  - RendererPoolFirst FUN_001a6078 never looks at the last slot (0x1A6084/0x1A60B8), and slot 11 then reads a renderer at 0
    (conv.).
- **C, harmless:**
  - ComputeSpotLightBounds takes the perpendicular of an unset sp+0x10 (0x1C8C18); the bounds are never read.
  - ModelNode::Update FUN_0019e5d0 compares a 24-bit count with 0xFFFFFFFF (0x19E7E0); dead branch.
- **P (data-dependent):**
  - Shader type 0x0A on a non-sky material draws with the last sky's matrix, and a sky part of another type reads a stale
    object block. All retail sky materials are 0x0A.
  - (conv.) FUN_001d5830 (uncalled) masks float colours as packed bytes. FUN_001ab800 writes its RET tag to address 0 for a
    model of no vertexes. Shader type 0x11's slot 11 recurses forever but is never called.

### Sound

- **C, reach unknown:** UpdateMusicEmitters FUN_001de488.
  - Bug: neither G_RendRel nor its camera is null-checked (0x1DE51C). With no camera it reads the chunk at 0xA0 (0x1DE52C),
    unlike UpdateInstanceSounds.
  - Effect: music emitters are culled against a garbage chunk.
- **P, reached:** UpdateCutsceneVolumes FUN_001df670.
  - Bug: fade -= time / fade (div.s 0x1DF6C0, sub 0x1DF6C4) divides by what's left rather than by a length.
  - Effect: the cutscene volume fade speeds up and ends after about 0.5 s.

### Saves

- **C, reached:** the save checksum (read check FUN_002a25c0, write FUN_002a2770).
  - Bug: the byte is loaded once before the loop (0x2A2678 / 0x2A2840) and the loop never reloads or advances it.
  - Effect: the checksum depends only on the size and the first byte. Corruption anywhere else in a bank goes undetected.
    Every save and load; both sides agree, so saves still load.
- **(conv.):**
  - IconTitleToSjis FUN_002a2920: a character outside the ranges before any digit or letter takes its code from past the
    ranges table. The game's own title never has one.
  - FUN_002a1c58 asks whether the card is formatted and drops the answer.
  - FUN_002a20a8 writes through a null item.
  - Ask FUN_002a08f8 returns the old result when asked again for the last operation.

### Loading and memory

- **(conv.), harmless:** FUN_0017a3c0, the chunk's own persistent-flag store's destructor, doesn't put its base vtable back
  (the other store's does).
- **C, reached:** String growth.
  - Bug: ConcatenateString (0x2048AC/0x2048B8), ReverseStringConcatenation (0x202520) and CreateStringFromChar (0x2047C8)
    set the capacity to 0 right after storing it.
  - Effect: every later growth reallocates. Heap churn only.
- **C, reached:** DiskSizeClass GetReadTableIndex 0x2037A8.
  - Bug: it tells class 10 from 11 by size < 0x100 (0x203824) where the size is at least 0x580.
  - Effect: class 10 is never used. None in practice.
- **C, harmless:** SoundTable::Construct FUN_00269b28 allocates the read-order array at 0x269B88 and again at 0x269BC8, leaking
  the first. It runs once at boot (the GameController constructor).
- **C, latent:** CopyAnimationData FUN_00299b20 releases the old disk node at 0x299B58 and again at 0x299BEC; the callers pass
  fresh data.
- **C, latent:** ReadTextFile FUN_0017e088. It counts every line break but skips blank lines when collecting
  (0x17E150-0x17E1A0), so the last k entries point past the text. Retail code-text files have 23 blank lines each, but those
  entries are presumably never used.
- **(conv.) latent:**
  - AddInstancePlace FUN_001988c0 has no cap at 8 places.
  - Object ID 0xFFFF reads through null in TakeObjects/ReleaseObjects FUN_00266180/FUN_00266260 and CreateInstanceFrom
    FUN_0025ecd0.
  - ResourcesStep doesn't check the voice queues.
  - GraphicsTable ReleaseAll/Release write through null.
  - RotationAt (cameras.cpp) reads before its keys.

### UI and text

- **C, latent:** LevelsPage::Construct FUN_001638c0. A world past 3 goes on with the levels/items on the stack unset
  (0x163970/0x163988). Only worlds 0-3 are made.

### Movies and cutscenes

- **C, reached:** ReleaseInstanceTrack FUN_0029f328.
  - Bug: it nulls the track's values (0x8, a 0x14-byte DynamicAnimationData from MemoryAllocate) at 0x29F394 without freeing.
  - Effect: every cutscene leaks 0x14 bytes plus a heap header per instance track.
- **C, reached:** QueueObjectMovie FUN_0029e5f8.
  - Bug: the music request word is read from the stack (ld 0x29E6F8). The track (0x29E6D8) and group (0x29E714/0x29E728) are
    set, but bits 20-31 are kept.
  - Effect: the loop bit 20 is a stack leftover, so whether the cutscene's music loops is chance.
- **C, reached when skipping:** PlayCameraTrackFrame FUN_0029a068.
  - Bug: it handles the end frame 0xFFFF for the values (0x29A0AC) but reads the cut channel at that frame anyway (mult at
    0x29A190, again at 0x29A3D0).
  - Effect: after a skip it reads 0xFFFF frames past the cut data. A value above 0 drops the end values, so the camera can be
    posed at another shot's first frame.
- **P (conv.):** VideoController::Reset FUN_0029e5a0 stops the cutscene while a pending reader still points at it (a
  write-after-free if a part were still loading).

### Maths

- **C, reached:** FindMinimum FUN_0018f3d0 (Brent's method).
  - Bug: when the point tried becomes the third best it stores previous = tried (0x24) but previousValue = secondValue (0x20)
    (0x18F75C-0x18F764). Numerical Recipes stores its own value (0x28).
  - Effect: slower or less accurate convergence.
  - Reached: spline cameras' nearest point, rigid bodies' ellipsoid test.
- **(conv.):**
  - TurnIkLink FUN_0018bc30 and ScaleIkTurn FUN_001847b0 compute a scaled turn and drop it (no link scales its turns).
  - P: FUN_00189dd0 stops a step short.
  - P: SegmentPointDistanceSquared FUN_00184ad0 measures to the infinite line (Cond581, 2 retail uses).
  - FUN_00188de8 (uncertain).
  - InvertMatrix writes its output as it goes (nobody inverts in place).

### Development-tool text parsers (dead code)

The parsers are vtable slot 2, ParseTokens. Retail never calls them:
- ReadScriptCommand reads command bytes straight from the stream; its slot-2 call at 0x206620 is the stream's Read.
- The token reader DiskDataReader_Init is only called by the parsers.

I checked Cmd595 in the asm: keyword 0xDD clears bit 1 (0x1117FC) where 0xDC sets bit 0. The rest are taken from the
conversion:
- Cmd583 overwrites the parsed values after its loop.
- Unbounded ID and byte lists: Cmd557 kinds 0x10/0x7B, Cmd582 kind 0x10, Cmd655 (16 ids for 12 slots), Cmd661,
  NextLinkedObjectInList, DoSound kind 0x16 (16 slots for 8), DoAnimation kind 0x11, AddTrail kind 0x16.
- Missing breaks: PositionWarp 0x58 -> 6, WarpAgent 0x58 -> 0x117, SetFocusObjectByte 0xB2, AddTrail 0x92 -> 0x17,
  ReleaseAgentRef2 0xFFFF, SetCollisions 0xD1, Cmd78 0xCA -> 0xC9.
- QueueObjectVideo 0x59 makes values above 1 into 0.01.
- AddTrail takes unknown keywords as clearing bits 10-13.
- Cmd44's 0x66/0x67 cases don't check the token's type.

## Sony SDK, newlib and GCC runtime bugs (linked in, not the game's code)

- **GCC 2.9x runtime, dead:** execute_cfa_insn (libgcc's frame.c, 0x17CB38) tests DW_CFA_advance_loc's bit (0x40) first, so
  DW_CFA_restore (0xC0) is taken for an advance and its own case is never reached. Nothing calls the unwinder.
- **libmpeg** (only checked in the asm: sceMpegAddStrCallback):
  - sceMpegAddStrCallback 0x2BC748 increments the count even when it replaced an existing entry (0x2BC860-0x2BC86C). The
    zeroed entry then matches every packet and calls address 0. The game adds each stream once.
  - (conv.) The interrupt handler 0x2B9468 reads the error flag before waiting.
  - (conv.) A picture of a multiple of 1,023 macroblocks (2,046+) gets an empty last chunk and can wait forever; the game's
    have 1,120.
  - (conv.) _sceMpegError 0x2BC9F0 reads sys->mpeg before the null check.
  - (conv.) _updateRefImage 0x2B7FE8 writes at 0x18-0x63 for a reserved picture_structure.
  - (conv.) _pictureHeader 0x2C0DC0 tests a 10-bit value for negative.
  - (conv.) _getAllRefs/_getRef0 make 6 references for 4 slots on dual-prime B macroblocks.
  - (conv.) dmaRefImage uses stale tags.
  - (conv.) The system's forcedBrokenLink (0xFC), which _updateRefImage reads, is only ever set to 0.
  - Effect: none with the game's streams.
- **Sony's movie decoder sample (the player's decoder), dead:** DecodePicture FUN_002b0a68 sizes each chunk's transfer by the
  pixels' size (`sll 6` or `sll 5`, 0x2B0DDC/0x2B0DE0) but always moves the picture on by 32 bit macroblocks (`sll 10` at
  0x2B0E08, added at 0x2B0E14), so a 16 bit picture of more than 1,023 macroblocks gets gaps. Only the IPU's own stream formats
  are decoded there; the game's movies are PSS.
- **newlib and scePrintf** (conv.):
  - strncpy's fast path mistakes bytes <= 0x7E for NUL.
  - toupper, tolower and strncasecmp index before _ctype_ for negative chars; strncasecmp returns the unsigned difference.
  - malloc accepts requests within 19 bytes of 4 GB.
  - malloc_extend_top takes an extra page.
  - _sbrk_r copies a stale errno.
  - expf leaves k unset on an impossible path.
  - snprintf pads %.Ns with '0'.
  - deci2Putchar prints a stale byte on lines of 126+ characters.

## Not a bug (dropped)

- **PlayMusicRequest 0x1DDFF0:** the old entry misread its delay slots. Retail always tests bit 19 and returns 0 without a
  player.
- **Cond584/585 "PlayerVisible2/3":** a peek-around-cover test that needs the direct line blocked by design. Only TT Lab's name
  suggests otherwise. 0 uses.
- **Names that don't match what the code does (TT Lab naming, not code bugs):**
  - Cond560 PlayerIsCrawling reads part bit 42 (0x12C214), the walk.
  - Cond566.
  - Cond595/597 are true for kinds 2/4.
  - Cond628.
  - Cond518 reads the shadow flag.
  - Cond50/51.
- **DisplayBottomText Cmd603/Cmd657 (0x120EA0/0x1129F0):** D_00309B14 isn't an unwritten table but the second entry of the
  text files' table g_Texts (D_00309B10, two files), which the language loader fills (language.cpp, `g_Texts[file]`); the
  label search missed the indexed write.
- **DoParticle's 64-bit read-modify-write of the word at 0x8** (0x21AA9C/0x21AAC0): it writes back the value it read.
- **ChunkData::particleView (0x110):** a per-frame scratch slot shared by the particles and sounds, each resetting it to -1.
- **Rollerbrawl Stop:** a random 0-360 taken as radians (0x152188) is still a random yaw.
- **Cmd171 object ID 0xFF as "none":** the scripts' own convention; all 231 uses write 255 for none.
- **Projectile Fly FUN_0010aa50:** the query allows 40 results for 32 slots, but its line-of-sight query keeps only the one it
  hits.
- **g_Rm2Queued:** reading older layouts until the first RM2 is queued is a format switch, not a bug.
- **TearDownCharacter destroying the locks inline:** code structure only.
- **Dead fetches with no consequence:** FUN_0012e328/FUN_0012fa90, Command 15, CreatureAgent::Collided's normalize,
  GetRotationVec results, Mecha's look targets 3/4, PlaceFeet and query leftover bits.
