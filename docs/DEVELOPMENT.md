# Working on the decomp

How the decomp is put together, how builds get tested and what replacing the game's code has to look out for. Setting up and
building are in the [README](../README.md).

## Symbols and the split

The asm's names come from the Ghidra project: `tools/ghidra/ExportProgramInfo.java`, run on the project's program, exports it
into `ghidra/` (not committed), and `tools/make_symbols.py` makes `symbol_addrs.txt` of it. `tools/split.py` splits the
executable with splat twice, so that every function spimdisasm finds gets a file of its own (see "A file is linked whole or not
at all" below), and `tools/fix_asm.py` fixes what splat gets wrong about the layout. After the names changed in Ghidra:

```sh
python tools/make_symbols.py       # ghidra/ -> symbol_addrs.txt
python tools/split.py              # symbol_addrs.txt -> asm/, then configure.py
python tools/build.py --matching   # the asm alone must still give the retail load image back
```

The tree can be copied to another folder or system as it is, `asm/` and `assets/` included (nothing in them has an absolute
path, `fix_asm.py` makes splat's `.incbin` paths relative), or split there again. Leave `build/` and `.venv/` behind.

`configure.py` writes `build.ninja` for the system it runs on (ninja runs the commands through `/bin/sh` on Linux and straight on
Windows, so their quoting differs) and `build/compile_commands.json` for editors. `tools/split.sh` and plain `ninja` still work
on Linux.

## Testing in PCSX2

These run on Linux, with PCSX2's Flatpak. `tools/play.py --boot-test` runs the first on the build with the disc image of
`local.json`.

`tools/run_pcsx2.py build/SLES_525.68.elf <PAL disc image>` boots the ELF in PCSX2's Flatpak and drives the game, through PINE
memory writes alone (no input devices), the way a player gets there: the logos, the title's "THREE YEARS AGO..." cutscene, a new
game (the only input, written into the game controller's next state after `--title-wait` seconds), the intro movie and playing the
beach; it prints the chunks loaded. `--quick` skips the logos, the cutscene and the movie, `--snapshot T` keeps a screenshot taken
T seconds in (from a save state it deletes again, never for the retail CRC), `--watch symbol[+offset]` prints a word whenever it
changes (`include/debug.h` has `DEBUG_STEP` for breadcrumbs), `--dump symbol[+offset]:words` prints words at the title and once
playing, and `--check-disk` walks the disk manager's blocks and checks them. On KDE Plasma a KWin script loaded for the run
(`tools/kwin.py`) puts PCSX2's window on one monitor (`--monitor NAME` or `$PCSX2_MONITOR`, HDMI-A-1 by default), below other
windows, and gives the focus back to the window that had it. The game gets a copy of the user's slot 1 memory card,
deleted afterwards. `--manual` lets the game go its own way (through `--press`, which holds pad 1's buttons through a hook of the
PS2 pad layer, `g_TestPadButtons`, and `--goto`, the game controller's next state): the main menu's New Game into the third save
slot is `--press 80:start:0.3 --press 85:cross:0.3 --press 93:up:0.3 --press 95:up:0.3 --press 97:cross:0.3 --press 100:up:0.3
--press 102:cross:0.3`, its Load Game of the fourth save `--press 80:start --press 84:down --press 86:cross --press 94:up --press
96:up --press 99:cross` (then `--press 140:cross` past the autosave notice), and `--goto 66:10 --goto 125:6` unloads and reloads
everything from the beach. `--card` plays with a card image of one's own, which `tools/mcread.py` lists and dumps files of;
`--loaders`, `--chunks` and `--watch` chains follow the loading, `--keep-state` and `--backtrace` look at a hang (the docstring
has the rest). Comparing a run with one of the asm alone (`build/matching`, a `--matching` build kept aside) is the test of a
replaced subsystem: the heap manager's counts and the disk manager's blocks come out the same. The heap starts at `_end`, so its
addresses and free space move with the executable's size (the C++ build ended 0xAE00 bytes past retail's 0x3DB200 on
2026-10-01, with 0x9AADB bytes of heap free at the beach): compare the counts, and keep an eye on the free space.

`tools/render_check.py` compares what a build draws with what the build before it drew, byte for byte: eight cases (a logo movie,
the title's cutscene, the main, options and load game menus, the beach standing, running into the water and paused) each stop
the game after a frame (`run_pcsx2.py --at-frame N:freeze`, the frame hook in `include/debug.h`, which also gives inputs at exact
frames) and keep its picture, `reference` keeps a build's as the ones to compare with. Every run plays with a fresh copy of one
card image (`build/render_reference/card.ps2`), which the game reads at the start and in the menus. Its runs use test time (`g_DebugFixedTime`, set in a copy
of the ELF, so it's on from the start): every frame takes the frame's time, a frame's background work gets 16 steps, reads are
waited for, and the heap's pools start at 0x400000 whatever the build's size. Without it two builds part within frames: loading
went as far as the frame's time left let it, reads came in at another frame, and the game goes through some of its objects in
the order of their addresses (two objects' random numbers swapped when the heap moved). The movies stay as long as the disc
makes them, so the gameplay cases start playing past the intro movie.

## Layout

| Path | What |
|---|---|
| `asm/text/<function>.s` | Every retail function's asm, one file each, named as in the Ghidra project: the matching build links them all, the C++ build none (every one is C++, PS2SDK's, or left out through `ps2sdk.txt`, `retired.txt` and `fragments.txt`) |
| `asm/data/` | `.data`, `.rodata`, `.sdata`, `.sbss`, `.bss` and the VU programs (`.vutext`), whole |
| `src/main.cpp` | Main and ParseArguments |
| `src/game/` | The rest of the game's C++: `memory.cpp` (the heap manager and the pools), `string.cpp`, `stream.cpp` (streams, files and memory streams), `filestream.cpp` (the file streams read on Platform::Stream's channels), `archive.cpp` (the BD/BH archives' tables: read from the BH by its section reader, sorted, searched; every path looked up is kept in a list for good), `readers.cpp` (the readers: the two storages of readers waiting on the two file streams, stepped a frame at a time or all at once, and the item reader classes that read a part of a file into the heap or the disk manager and hand it to a section reader), `gamecontext.cpp` (GameContext's start-up of the game's systems), `chunkloading.cpp` (the chunk loading manager: a loader per chunk with a state machine for each of its two files, RM2 and SM2, that loads what's wanted and unloads what isn't, the links followed from the focus chunk at the depths that decide what's wanted, through link hulls while the focus object is inside them; the SM2's loader makes the chunk's data and reads the SM2 into it, the RM2's adds the chunk to the chunk manager and reads the RM2), `chunkdata.cpp` (a chunk's data: made by the SM2's loader, its parts (scenery, the instances' contexts, lights, collision, particles, dynamic scenery) made as they're read, its frame while it's shown (`UpdateChunks` steps every chunk's), and its release, a part at a time through the readers or at once; the list of every chunk's data and the links each keeps a list of), `chunkfiles.cpp` (the items the SM2 and RM2 are read by: the SM2's scenery, dynamic scenery and links into the chunk's data and its graphics queued, the RM2's sections handed to the code, graphics and instances' readers), `reference.cpp` (the objects' reference counts), `math.cpp` (the game's maths library: matrixes, vectors, sines and cosines (`Platform::Math`: VU0's microprogram on the PS2), the curves of points, the random number generators), `save.cpp` (the save's date), `savedevice.cpp` (the save code's memory card device: the save's files and the requests on them, asked for and polled by the save code, on Platform::Saves; the space a save of its files needs), `shapes.cpp` (the UI's 2D shapes, their vtables the retail ones: sprites (a texture's area on the unit square a matrix places, turned or not), strips of coloured vertexes and rings (strips between two ellipses of segments, shaped and coloured by curves, inside each other), drawn through `Platform::Graphics`, read from a stream with their resources), `particles2d.cpp` (the UI's 2D particles: a pool of 16 byte slots with a free list, the emitters that make particles between a widget's places and age them, drawn as a sprite sized, turned and coloured by curves of their age, and the radial ones thrown out of the middle), `bindings.cpp` (the button bindings: up to four buttons an action, a modifier action held or not, pressed this frame or held, and how hard), `resources.cpp` (a texture's and a material's resource from their tables or read from a stream), `graphicstables.cpp` (the tables of the graphics resources the chunks share by ID (textures, materials, models, rigid models, skins, blend skins, meshes, LODs, skies): one template for the retail tables' shared code (sorted by ID, searched by halves, grown by 0x40, references counted, released resources queued for ResourcesStep to delete), each kind's three vtables' functions under their retail names, the references a chunk being read takes and lets go of once it's read; the graphics section's readers, a kind's subsection at a time, and the skies' own files; each kind's representation is `Platform::Graphics`'), `array.cpp` (the pointer arrays' retail iterators), `font.cpp` (a font read from its PSF file, and a text laid out in lines of its glyphs), `overlay.cpp` (a renderer's 2D overlay for the frame: shapes queued in six layers, placed by a matrix or not, in a colour or their own, texts queued per font, all drawn at the end of the renderer's frame and dropped), `colour.cpp` (the game's colours as fractions: red, green and blue 1 at 192, alpha at 128), `widgets.cpp` (the UI's widgets, their vtables the retail ones: a state machine of hidden, appearing, shown and disappearing over timed durations and holds, chained so that what one is told passes on down the chain; animated widgets lerp colour, place and scale between hidden and shown values, labels draw a text of the text table or a string of their own, tiled pictures eight sprites, sprite widgets a sprite with a drop shadow, a pulse, a bob and a wobble, and their 2D particles; menu widgets step and draw a menu, ring widgets rings; the UI controllers' base keeps 64 widgets that 64 bit masks show and hide), `menus.cpp` (the UI's menus: pages of items with a selection per player, the pages next to each (left and right in rings, above, below, the parent), entered and left with their items told, a frame of a page that moves the selection or goes to another page from the menu input (seven actions of the button bindings, pressed and held) with the menu's sounds), `widgeteffects.cpp` (the effects widgets play: a curve's scale, sparkles of 2D particles, rocking, sliding, spinning), `oleg.cpp` (OLEG, the in-game UI manager, a member of the game controller: its widgets (the HUD, the pause menu and the game's progress round it, the front end's menus, the save manager's screens, the loading and legal screens) made and set up in the retail order and destroyed, its start-up (the menus' pages, the menu input's buttons, the drawers' styles, the front end's sounds), its frame (the wumpa fruit added one at a time, a hundred making a life, the pickups' effects), the pictures it reads from files (the Crash title, the levels' titles, the legal, loading, game over and credits screens), the health bar, the slider and the widgets of the levels' and the save slots' pages, the HUD's values each frame and the particles' material), `olegpages.cpp` (OLEG's menu pages: the main menu, the options and their three pages, the screen position, the pause menu, the quit and autosave questions, the notices, the game over, the levels pages and the extras with their galleries and movies, the actions they ask the game controller for; the save slots and save choices pages are `savepages.cpp`), `gamecontroller.cpp` (the game controller: the game's flow as 23 states stepped every frame (the start-up, the logos, the title, the menus, loading a level, playing, watching, movies, pausing, the gallery, the game over, the credits, restarting), the saving's steps around the save code, the characters' roles, the frame's drawing, and the scripts' requests (the bottom text, cutscenes, boss mode, whack-a-worm, autosaves at checkpoints, game over, credits)), `progress.cpp` (the game's progress: the counts, the play's state (mode, pairing, characters, areas), the time played, the levels' gems and crystals, the checkpoints play starts from again (an instance, its chunk's persistent flag and where the characters come back to) and the ways into the game that reset them; the save controller that a save's file is read into and written from, and what it takes of the game and gives back), `language.cpp` (the languages' text files, their lines and the texts in use), `instances.cpp` (the game's object model: the objects references point at (asleep, queued to be stepped, released; their place and collision), the instances' contexts with their nodes (one of each of 24 kinds, each a class of the game with its vtable), the events queued for an instance and handed to its nodes of the kinds each goes to, each chunk's instances (the sleeping ones, and a list of the nodes of each kind, stepped kind by kind in the game's order with the nodes taken out meanwhile taken out after), the instances of no chunk, the objects queued for a step, what's freed a thing at a time, the instances' IDs), `properties.cpp` (the instances' properties: the lists an RM2 has of tagged values, floats and integers, the holders each class keeps them in and the extras beyond them), `objects.cpp` (the game's objects as the RM2's code section has them: header, name, properties, the resources they name, the script pack and the slots), `resourcetables.cpp` (the tables of the game's resources by ID, one per kind and a voices' table per language, the code section's readers that fill them and the unloading of what the chunks no longer use), `instancesection.cpp` (the RM2's instance sections: each layout's item and its nine kinds' items and section readers (a layout's sections go templates, AI positions, AI paths, positions, paths, surfaces, instances, triggers, cameras), every element read from its section and registered with the layout: templates to the instance factory, object instances given contexts and their waypoints, AI positions and paths to the chunk's AI navigation, positions and paths to the chunk's lists, triggers and cameras given contexts that tell their instances, plain boxes for the sound code, collision surfaces copied into the game's table; once read, the instances linked to the ones they name), `layout.cpp` (a layout's elements: object instances, instance templates, triggers (message triggers and cameras), the sound boxes, positions, paths and collision surfaces), `navigation.cpp` (a chunk's AI navigation: its layouts' AI positions and paths, their searches and links, the path finder's A*-like search over them and the routes it makes), `instancefactory.cpp` (the instance factory: an instance's context made from its object instance (its place, its model's, object's and type's nodes; an agent of its object's type with its class's property holder and part), and the contexts of triggers and cameras), `agentnodes.cpp` (the agent nodes, kinds 0xC to 0x14: the node an instance's agent is reached by, which hands it events, frames and chunk changes), `agentparts.cpp` (the part of its type an agent keeps (retail's InstanceCreationHelper classes): the last attack that reached it and when, and the bits of what the agent's state gives it and of the attacks that reach it), `agents.cpp` (the agents (retail's ObjectInstanceContext classes) of the object types: the base made with its object's resources taken and let go of, the basic agent's state applied to its instance, its contact messages, attacks and launches; each type's constructor, destructor and small functions; the playable characters', the crates', creatures' and generic objects' larger functions are in their own files), `objectresources.cpp` (the resources an object's references list taken by an agent: a reference each, made empty in their table when it has none yet, and let go of into the tables' deletion queues), `commands.cpp` and `conditions.cpp` (the script commands and conditions the object builder makes, a class each (`include/game/commands.h`, `conditions.h`, made once by a script from the retail builders and TT Lab's AgentLabDefsPS2.json, whose names and arguments they have, and edited by hand since): the builders, the destructors and sizes, and the commands' executions that only run their execution on the agent's node), `commandsnode.cpp`, `commandsagents.cpp` and `commandsgame.cpp` (the commands' executions that are no subject's of their own (see "The rest of `src/`"): what works on the agent's object node, its runner, levels, focus, keys and routes, perceptions, head tracking, motion block, rigid body, linked objects, crates and creatures; the agents' parts, the characters, the linked objects' events and the follow camera; and what asks the game controller (lives, boss mode, whack-a-worm, the bottom text), the video controller, the music and the playable character), `conditionchecks.cpp` (the conditions' checks), `agentlab.cpp` (the behaviour scripts as the RM2 has them: starters and their assigners, graphs of states, bodies, conditions, commands and control packets, and what a packet's values read), `behaviours.cpp` (the behaviour scripts at work: an agent's runner, its stack of levels each running a graph's states (the best body by its condition's score, the completion body once the packet or child behaviour ends, the interrupting states), the starters' receivers and the call conventions that find them), `motion.cpp` (an object instance's motion from its control packets: the packet's start (its time, what it goes to, its translation (straight, accelerated, a spring, a throw, a chase), its rotation and turn), its frame (the target followed, the steps, facing the way it moves or rolling, the end by both parts done, the delay or the sync), the waypoints (keys, paths and routes), the motion's parts and the steps: straight and accelerated moves and turns, the
interpolations, springs, throws and the four chases (on the ground, in the air, riding a body of the physics, climbing what it
touches), the steering toward a target (by turning or by a rigid body's push) and the rolling along the ground; the trajectory
controller is `trajectory.cpp`), `objectnode.cpp` (the object instances' node, kind 1:
its frame (stepped less often the longer its instance goes unseen: its runners, its movement and its rigid body), its parts made
and let go, the events it handles (a trigger message starts the behaviour its object lists for it), the designators the scripts
name (AgentRef1 and AgentRef2, the focus, the head tracking's target, the stored position: the retail code gives AgentRef1's
position for AgentRef2's and the head target's), being launched, pushed and knocked, and the contact sounds of the surfaces it
lands on), `animation.cpp` (the skeletons' animations: the animations and OGIs read from the RM2's code section, the animations
played on an instance's joints (each joint's chain of statuses started, blended in and out, looped or ended), each joint's pose of
a frame from its tracks (a static value or this frame's and the next's by the share, turns slerped or added, on VU0's
microprograms through `Platform::Math`), the joints' matrices for the skins in the frame's memory, the blend shapes' weights in a
ring, the exit points' matrices on their joints and the callbacks of the camera's joints), `ogi.cpp` (the instances' models drawn:
the instances the view test queued, each through its chunk's matrix with the strongest lights at it, an OGI's rigid models on
their joints and its skin and blend skin (their joints' matrices taken from the bind pose, the three first shapes with weights)
through `Platform::Graphics`), `shaderanimation.cpp` (the materials' shader animations: read, played in a loop and sampled into
their UV offset and colour), `dynamicscenery.cpp` (the dynamic scenery's models read from the SM2: their hulls and their
animations of float values), `lights.cpp` (a chunk's lights: the scenery's ambient, directional, point and spot lights, their
vtables the retail ones, read from the SM2, and what they come to at an object being drawn: the ambient lights summed, the three
strongest of the others (by their fall off, attenuation and cone) with the object's own light in the third place when it has one,
their directions turned through the chunk's matrix), `collision.cpp` (a chunk's collision: its tree of boxes, groups of
triangles and vertexes read into the disk manager, the box queries (the leaves found go into the scripts' state jump table's
second half), the precise ray cast (every triangle of the leaves the ray gets into, on the CPU) and the fast one (the triangles
pipelined through `Platform::Math`'s ray tests, VU0's microprogram 0xAB8 on the PS2), the triangles touching a box, the
segments cast through the collision and the instances' cells (the line of sight the scripts' conditions use, the camera's and
others' casts; the retail code takes the stop point of the collision mirrored), the surfaces' sounds and particles of each
contact kind, the game's table of surfaces and their defaults, the caches of the triangles near an object (gathered again once
the object leaves their box), spheres and ellipsoids against triangles (whether they touch and where the triangle pushes them);
VU0's macro helpers (planes, a triangle's edge tests and box) are `src/platform/ps2/collisionmaths.cpp`), `hull.cpp` (convex hulls: read with their blob of
vertexes, planes, separating axes, faces and edges, a point inside one, which side of a plane one is, two meeting by the
separating axis test, and the builder that makes one of points and faces (the edges, edge directions, outward planes and face
normals worked out, merged as the game does, and packed): boxes, triangles and pyramids), `physics.cpp` (the character's collision
solver: the contacts it gathers around a moving body (the triangles and instances' hulls and spheres near it, each made the space
a box hull at the origin can't go into: two planes per separating axis, worked out on VU0 and cached for triangles), the body
pushed out of them, slid along the planes and creases it crosses a step at a time, the ground found under it or probed for
around it, the steps up ledges and what it lands on; two hulls' and a triangle's and a hull's contacts by the separating axis
of least overlap, a sphere's and an ellipsoid's against a hull's planes, faces, edges and corners), `rigidbody.cpp` (the rigid
bodies: a box's mass and inertia, momentum, rotation and angular momentum stepped in substeps with forces, springs, damping and
constraints (a hinge, a fixed point, a line, a plane, a rotation limit), the impulses of contacts with surfaces, instances and
other bodies, spheres and ellipsoids (the ellipsoids' test the retail search over their polynomial) and bodies of hulls
colliding with the chunk's triangles, the instances around them and each other, floating on water, riding a trajectory, and
the world of 200 slots that steps every awake body a frame), `objectcollision.cpp` (an object's collision: its hulls (a box hull
of its own, its OGI's at their joints or its kind 4 node's), their surfaces and placed matrices, its boxes, its frame, and the
instance queries' filters), `place.cpp` (an object's place: its matrix and its position and rotation, each made again from the other side once that changed), `view.cpp` (the render view: the camera's projection from its lens, the matrices to camera, clip and screen space), `cameras.cpp` (the camera triggers' cameras: the main camera's values and its two subtypes (points, lines, paths, splines, zones, the boss camera's arena), where each puts the camera for a target or at a parameter, read from the RM2; the camera triggers' nodes, which send what enters their box and the instances they tell the camera's event; the keyed camera's play along its spline), `camerablender.cpp` (the follow camera's blenders: an angle's or a distance's value eased toward a goal (a value, a range's ends blended by a share, a second range) at a speed or by an input, held, pushed and kept within ends), `camerarig.cpp` (the camera rigs: a lens's rig and the blend to the next, a rig's target, positioner and the point followers that smooth them, the shake, the cutscenes' rig and the game's (the frame its commands' two places make, the scripted target and positioner the cutscenes' commands move from a place to another, along a path or arcing round the target, eased in and out)), `followcamera.cpp` (the player's camera: the positioner behind the target at a pitch, yaw, field of view and distance taken from the camera triggers (blended over their time, or the second subtype's place), four probes around the view that turn it and pull it in from walls, pushed off the collision and out of the instances' hulls, moved to a clear place once the view stays blocked, tilted toward the target's facing; the target that follows the player's place with its height eased, a box's point above it and the trigger's first subtype's point), `movie.cpp` (the movies' controller: the game's requests to play and stop taken on once a frame, the start with the screen, the sound and a music stream's buffer handed to the player and the end that gives them back and restarts the renderer, where the picture goes on screen for the movie's and the TV's shapes, the file's name from the platform's conventions; the playing is Platform::Movie's), `renderer.cpp` (the renderer's frames: the render buckets sent to be drawn and started again, which Platform::Graphics does), `particles.cpp` (the particles: the systems and emitters read from a chunk's particle section (the old versions' values made the current ones), the emitters' runtimes (their blocks of 32 particles or 12 distorting hexagons, a wheel of 32 frames they're due in, their on and off cycles and the camera's distance that switches them), the twelve generators and six velocity rules that make a particle (its start, velocity and spawn time, its ghosts), the events of the blocks on another wheel of 32 frames (bounces off the emitter's plane and vertical plane, blocks let go of, chains ended and blocks made free), the collision spheres objects test against, the start-up (system 0 the "null" system, the wave shader and the distortion's material through `Platform::Graphics`) and killing every particle the game started, a system's render table of 64 steps of its life, and the draw lists' blocks drawn in their chunk's view (the views' VU0 microprograms through `Platform::Math`); the three texture pages loaded from their startup files or read from the default chunk's RM2 (made by `Platform::Graphics`), and an RM2's particle section), `decals.cpp` (the decals of the default particle data: a pool of 32 blocks of 32 decals by type and key, the decals added (their frames made orthonormal, their places taken into the camera's chunk), read from the default chunk's section, aged every frame by `Platform::Graphics::AgeDecal` (VU0's microprograms on the PS2, a block's decals by its first decal's variant as retail has it), dropped once their life is over and put in their keys' draw lists; their texture page and the two default types' UV rectangles), `sound.cpp` (the sound code's side of Platform::Audio), `disk.cpp` (the disk manager), `clock.cpp`, `context.cpp` (the frame loop's engine side), `pads.cpp` (the controllers) |
| `src/abi.cpp`, `include/abi.h` | The calls between the retail convention and the C++'s |
| `src/platform/<platform>/` | The platform layer's side for one platform (`ps2/`: PS2SDK, the entry point; `ps2/renderer/` the renderer's PS2 side: `buckets.cpp` the DMA chains and the 28 render buckets written into them, linked into one chain for VIF1 every frame, `frame.cpp` the frame's set-up packet, `vuprograms.cpp` the VU1 programs' uploads, `textures.cpp` the GS memory's texture slots and the textures' registers, `materials.cpp` a material's set-up (its programs, the VU1 buffers taking turns, its shaders) and the frame's materials put into their buckets, `shaders.cpp` the shader types' VU1 data, `models.cpp` the instance blocks VU1 reads several instances of a model from, rigid models and placed ones (dynamic scenery, billboards), `skins.cpp` skins and blend skins with their shapes, `resources.cpp` the graphics resources as the PS2 has them (textures' and models' packets in the disk manager's blocks, materials' shaders, skins' and blend skins' packets and shapes, rigid models and the scenery's meshes, LODs and skies taking their materials, models and meshes from the tables): each kind made, deleted and read, `sky.cpp` the sky drawn first in its half size buffer, `particles.cpp` the particles' and decals' VU1 blocks, the texture pages' four blend modes' materials (shader types 0x12, and 0x13 with STQ coordinates for the decals) made from the page material's first shader, the systems' render tables, the chunks' views loaded into VU0 for the frame's particles and the decals' view, types and aging on VU0, `shadows.cpp` the shadows' half size buffer, `effects.cpp` the screen effects (the depth copied into a buffer the colour filter's palette reads), `text.cpp` the fonts' glyphs for VU1 to expand, `draw2d.cpp` the UI's 2D shapes as GS primitives) |
| `include/platform/` | The platform layer's interfaces |
| `include/game/` | The game's types and the asm functions C++ calls |
| `include/retail/` | The game's C library calls by the retail names (`src/platform/ps2/libc.cpp`, malloc.cpp, libm.cpp and the toolchain's newlib, see "The C library") |
| `include/gcc2.h` | GCC 2.9x's C++ ABI: vtables, virtual calls, destructor flags |
| `ps2sdk.txt` | The Sony SDK functions left out of the link (PS2SDK or the platform layer have them) |
| `retired.txt` | The game's functions left out of the link whose work the C++ does under other names (the PS2 start-up, the timer, the disk manager's list helpers) |
| `fragments.txt` | Bytes between functions that splat split off as functions but nothing reaches (stray epilogues, dead stores, padding after a function's last jump), left out of the link |
| `tools/` | The split, the linker script, the checks and the PCSX2 runner |
| `.clangd` | clangd's flags for the C++ (the README's editor setup) |
| `local.json` | This machine's paths (git-ignored, `tools/local_config.py` reads it; `local.example.json` shows it) |

`configure.py` compiles `src/**/*.cpp` (the platform side picked by `PLATFORM`, `ps2` by default). A function a C++ file defines
(by its asm name) isn't linked from asm: `tools/make_ld.py` leaves its file out and lays the rest out in the retail order,
followed by the C++ and PS2SDK. The layout moves wherever code is replaced, so everything has to be symbolic: `make_symbols.py`
makes symbols of every pointer it can find (Ghidra's typed pointers, jump tables, vtables and callbacks in data).

### The rest of `src/`

The files the table doesn't name, by what they hold (each file's own comment at its top says more):

- **The playable characters**: `characters.cpp` (their agents' controllers), `charactercontrollers.cpp` (the controllers'
  module start-up), `characterframe.cpp` (a character's frame: contact messages, touches, launches, attacks), `characterstate.cpp`
  (falls, long drops, hits and the other states' work), `charactermovement.cpp` (the ground movement: the velocity eased by the
  surface's grip, pushes, spins and slides), `charactermoves.cpp` (the moves' checks for room), `characterwalk.cpp` (the walk:
  turning, strafing, being pushed), `charactersolver.cpp` (the moves through the collision solver: the limbs' probes, riding a
  hull, what it stands on, the tied characters), `characterlink.cpp` (the tied characters' arms as an IK chain),
  `characterlook.cpp` (the look shared out over the head and spine), `characternodes.cpp` (the controls and the follow camera
  nodes), `controls.cpp` (the pad read into a character's controls), `followrig.cpp` (the follow camera's rig: its points, probes
  and distance), `proceduraljoints.cpp` (legs on slopes, dangling, the landing's squash), `targetlock.cpp`, `claw.cpp` (Nina's
  claw), `gun.cpp` (Cortex's and the Mecha-Bandicoot's gun), `graplerope.cpp` (the graple's rope and ragdoll), `skidmarks.cpp`.
- **Vehicles**: `rollerbrawl.cpp`, `humiliskate.cpp`, `wrestle.cpp`, `hoverboard.cpp`, `vehicles.cpp` (the wall cling and what
  they share).
- **Object nodes**: `objectnodeparts.cpp`, `objectnodehelpers.cpp`, `objectnodemotion.cpp` (launches, the motion blocks' cycles),
  `nodeparts.cpp`, `nodecontrollers.cpp` (command 645's controllers), `headtracking.cpp`, `perception.cpp`, `particletrails.cpp`,
  `rigidbodyframe.cpp` (a node's rigid body over a frame), `chunkrigidbodies.cpp` (a chunk's lists of rigid bodies),
  `waypointroutes.cpp`, `movementnode.cpp`, `attachment.cpp` and `attachments.cpp` (instances held by another, on AI positions or
  springs, and the node of kind 6), `instanceplacement.cpp` (where an instance is kept to be put back), `modelinstances.cpp`
  (model nodes of a model's ID, the instances made outside the layouts), `pickups.cpp`, `projectiles.cpp`, `springbody.cpp`
  (spring bodies and skeletons), `messagetriggers.cpp` and `triggernodes.cpp` (the triggers' nodes and the instances they gather),
  `instancedecals.cpp`, `instanceparticles.cpp`, `soundobjects.cpp` (the sounds played on instances, the music emitters).
- **The scripts**: `behaviourevents.cpp`, `behaviourmodule.cpp`, `eventcallers.cpp`, `agentlabitems.cpp`, `scriptpacks.cpp`, the
  commands' executions by subject (`commandsattach.cpp`, `commandscamera.cpp`, `commandscharacters.cpp`, `commandscontrollers.cpp`,
  `commandsfocus.cpp`, `commandsmotion.cpp`, `commandsphysics.cpp`, `commandsroutes.cpp`), and the development tools' text
  parsers retail never calls (`commandtokens*.cpp`, `scripttokens.cpp`, `scripttokenhelpers.cpp`).
- **Chunks and items**: `gamechunkmanager.cpp` (the game's chunk manager and the chunks' persistent flags), `chunkscenery.cpp`
  (the awake instances sorted into collision cells), `chunkview.cpp`, `chunkwind.cpp`, `dynamicsceneryrun.cpp` (the dynamic
  scenery's models at work), `sceneryitems.cpp`, `objectbuilder.cpp`, `layoutiterators.cpp`, `listiterators.cpp`, `pools.cpp`
  (the item pools every item type has a copy of), `rendererpool.cpp`, `unusedreaders.cpp` and `textconsole.cpp` (classes nothing
  makes), `copyprotection.cpp`.
- **Cutscenes and saves**: `cutsceneplayer.cpp`, `cutscenereader.cpp`, `cutscenetracks.cpp`, `videocontroller.cpp`;
  `savecode.cpp` (the state machine the game saves and loads through), `savefiles.cpp` (a save's files: the base file with its
  checksum, the icon, the card folder's summaries, icon.sys), `savemanager.cpp`, `savepages.cpp`, `savecodeselection.cpp`.
- **Maths and geometry**: `curves.cpp` and `curvesearch.cpp` (the layouts' paths and the camera splines), `rotations.cpp`,
  `segments.cpp`, `matrixinverse.cpp`, `ikchain.cpp` and `iksolver.cpp` (the IK chains and their solver), `volumes.cpp` (spheres,
  segments and hulls tested against each other).
- **The PS2 side** (`src/platform/ps2/`, besides what "The platform layer" below describes): `entry.cpp`, `system.cpp`, `io.cpp`,
  `disc.cpp`, `stack.cpp` (running a call on a stack in main memory), `time.cpp`, `audio.cpp`, `memorycard.cpp`, `bounds.cpp` and
  `matrices.cpp` (VU0's macro mode maths), `collisionmaths.cpp`, `sdr.cpp`, `libc.cpp`, `malloc.cpp`, `libm.cpp`, `multistream/`,
  `movie/` (`decoder.cpp`, `player.cpp` and libmpeg's and libipu's files), and in `renderer/`: `culling.cpp`, `dma.cpp`,
  `screenmodels.cpp`, `shaderclasses.cpp`, `shadersettings.cpp`, `shadertypes.cpp`, `vu0programs.cpp`. `src/debug.cpp` has the
  test hooks the PCSX2 tools read and write. `src/gcc2.cpp` is GCC 2.9x's runtime the game has (`__main`, `__pure_virtual`, the
  unwinder's call frame interpreter).

## The platform layer

The game's C++ calls `Platform::System` (the start-up: on the PS2 the device resets, the I/O processor's restart with the
disc's IOPRP image and its drivers; the console's language and local time, libscf's logic on the PS2: its clock keeps Japan's
time), `Time` (a clock of 576000 ticks a second, the PS2's timer 0), `Memory` (the pools, the cache
and the ordering of writes hardware reads), `Files` (by paths on the disc, the PS2 side makes them `cdrom0:\...;1`), `Io` (the I/O
processor's heap), `Disc`, `Pads` (DualShock 2s: another platform presents its pads as one, modes, pressures and report
included), `Saves`, `Sound`, `Stream`, `Audio`, `Movie` and `Graphics` (`include/platform/`; `Graphics` draws the UI's 2D shapes (sprites in fractions of the screen, strips in pixels) and the scene's models (rigid models,
skins and blend skins, lit by the three strongest lights) and presents the renderer's frames: the wait for the vertical blank, the chain of the frame's render buckets sent to the GIF, the display's PCRTC set up from the renderer's settings; and it switches the VU0 microcode sets the game's maths calls into, `vcallms`), `Math` (the sines and cosines the game's results depend on to the last bit: VU0's microprogram on the PS2, its polynomial on the CPU for the others; and the animations' joints, VU0's microprograms on the PS2: slerps, Euler rotations, turns and joint matrices; the collision's ray
against triangle tests). `Stream` reads the disc's files in the background on numbered channels (opened by path, read into memory or into the sound processor's as a sound bank's samples, polled or waited for) and follows the disc's state once a frame (an open tray holds reading until a file of the disc opens again); the PS2 side is SCEE's MultiStream, the EE client of the disc's STREAM.IRX (`src/platform/ps2/multistream/`: commands batched for the IOP's RPC server 0x12345 and sent once a frame, the module's status parsed out of every reply, the EE's own server 0x12344321 for the module's calls back). The game's file streams (`src/game/filestream.cpp`, a pool of streams whose slots are the channels, each with a reader buffering what's read in parts, a disc file or the files of a BD/BH archive) are on it. `Audio` is the sound processor in the SPU2's terms (48 voices numbered core * 24 + voice, 14 bit volumes, pitches of 0x1000, the 10 reverb modes, volume groups, music streamed on `Stream`'s channels into one voice or two interleaved); `src/game/sound.cpp` has every function of the game's sound code that called MultiStream (voices, reverbs, groups, the music players, the movie's hand-over), so no asm left calls it. `Saves` is the storage of the game's saves by port and slot, a save a directory of files named by the game's region and product codes: one operation at a time (format, make a save's directory, measure a save, find a file, read and write one), started and moved on by an Update a frame, its result asked for after. The PS2 side (`src/platform/ps2/saves.cpp`) is the retail memory card manager, a function of steps an operation each starting a libmc call the non-blocking sync finds done, on libmc (`src/platform/ps2/memorycard.h`, the PS2 side's own now). `Movie` plays a movie file for the game's controller (`src/platform/ps2/movie/`: Sony's movie sample code the game built its player on, in C++: the PSS file streamed from the disc with `sceCdSt*`, demultiplexed by libmpeg into a video ring for the IPU and a sound ring, pictures decoded with the IPU (libmpeg, or the IPU's own formats 0 and 1) while the main thread's stack moves out of the scratchpad libmpeg works in, uploaded to the GS's Z buffer memory by a DMA chain sent from the vertical blank's interrupt every second blank, drawn as 32 pixel strips into render bucket 27, and the sound sent by SIF DMA into a ring in the IOP's memory that the SPU2's block transfer plays in a loop; libmpeg and libipu are the platform's own C++ of Sony's libraries, `mpeg.h`, libsdr's EE side is `src/platform/ps2/sdr.cpp`). What the IOP does with MultiStream's commands was read off STREAM.IRX (its command switch at 0x8DD0, the table at 0x12BFC, libsd's imports from 0x12A8C). A platform implements them in
`src/platform/<platform>/` along with its entry point, which sets the machine up and calls `Main`. Values the game goes by (pad
states, the save operations' numbers, open flags) are the PS2's, and the PS2's peculiarities stay in `src/platform/ps2/`.
Another platform needs its side of the layer, and the game's C++ that still builds PS2 hardware packets itself (the
renderer's VIF and GIF packets and DMA chains, VU0 and VU1 code) moved behind it.

## Calling conventions

The retail code is GCC 2.9x's EABI with 64 bit registers; the C++ is PS2SDK's n32. The asm is assembled as n32 (the linker takes one
ABI; splat writes n32's register names, `$a4`-`$a7` for `$8`-`$11`) and keeps its own conventions inside. Across the two:

- integer and pointer arguments and results are passed alike (`$4`-`$11`, `$2`);
- floats aren't: EABI puts the Nth float argument in `$f12+N`, n32 an argument in the register of its position. A function
  taking both goes through a thunk that `include/abi.h` makes from its C++ declaration at compile time (C++26's asm statements of
  constant expressions): the C++ function is named `<retail name>_n32` (`RETAIL_N32`), `EABI_EXPORT` gives the asm the retail name
  and `EABI_IMPORT` gives the C++ an asm function. Calls through the retail vtables and function pointers go through
  `Abi::CallEabi` (`CallVirtual` does it by itself for such arguments);
- the retail code expects `$f20`-`$f31` kept across calls, n32 only the even ones: the C++ is built with `-ffixed-$f21` ...
  (GCC 15 takes `-fcall-saved-$f21` ... and still uses the odd ones without saving them);
- structs passed or returned by value differ too, and about 45 retail functions keep 128 bit values in saved registers, of which
  GCC only saves 64 bits: check the callers before replacing a function they call;
- the stack stays 16 byte aligned (n32's), which the retail code's `lq`/`sq` need; GCC's EABI only keeps 8;
- divisions don't trap on 0 (`-mno-check-zero-division`), the retail ones don't.

Classes with a vtable (a GCC 2.9x one, still in the retail data) are C++ classes whose methods carry the asm's names
(`RETAIL(FUN_...)`); their vtable pointer stays where GCC 2.9x put it and virtual calls go through `CallVirtual`. The base class
has the virtual calls (inline, by slot) and a derived class's methods of the same names are its versions, which its vtable points
at: `Stream::Read` calls through the vtable, `File::Read` is the file's. Types without one are structs with free functions.

The C++ keeps the retail behaviour where it shows, bugs included (a string's capacity reset after it grows, the disk manager's
size class 10 that nothing reaches, a new block's bits left from the allocator): the heap and the disk pool come out the same as
the retail game's, which is what the runs compare.

## Writing the C++

Names say what the code does with a value, found by reading every use (all of the game is C++, so a grep finds them); the
retail names stay as link names (`RETAIL(FUN_...)`, a global's `RETAIL(D_...)`). Types, functions, constants and enumerators
are PascalCase, fields and locals camelCase, globals `g_`. A field nothing reads, written or not, is `unusedXX` (its offset in
hex). Fields are never removed and layouts never change: a struct the game's
files, the retail data or the retail code lay out has its `CHECK_OFFSET`s and `CHECK_SIZE`.

- **Words of bits** (flags, packed values, a command's packed arguments, hardware registers) are unions of the whole word and
  an anonymous struct of bit-fields from bit 0 up, every bit named (`unusedN` for what nothing reads), with a `CHECK_SIZE`:

  ```cpp
  union ReferenceBits
  {
      u32 value;
      struct
      {
          u32 count : 24;
          u32 owns : 1;
          u32 unused25 : 7;
      };
  };
  ```

  The code reads and writes the fields (`instance->flags.visible`). A word written whole (a register, a GIF tag) is made in a
  local union and stored as `.value`. Where the game ORs a value into a word without masking it, the code keeps a shift to the
  field's named position: a bit-field would cut off what the retail code lets through. The words of bits a query takes (wanted,
  unwanted) are masks named next to the union (`ReferencedObjectFlags::Visible`).
- **Constants**: no bare numbers but the obvious ones. A constant one file uses stays in its anonymous namespace; one several
  files use has one definition, in the header of what it describes: a vtable's slots in its class's `enum Slot`
  (`Agent::ContactSlot`, `ObjectNode::TakesPacketsSlot`), a field's values next to the field, an ID's "none" next to the ID's type,
  node kinds and their masks in `instances.h`'s `NodeKind`, the colour table's indexes in `colour.h`, the maths in `math.h`
  (`Pi`, `Epsilon`, `Infinite`, the turn angles). An enumerator declared in a class silently shadows a file-level constant of
  the same name in its subclasses' members, which changes code: a shared definition goes in with the local copies gone.
- **GS registers** are libgs.h's structs (`GS_TEXA{.alpha_1 = 0x80}`) made into words with `std::bit_cast`. Constants made that
  way are `const`, not `constexpr` (clang can't evaluate a bit_cast of bit-fields, GCC folds both). libgs.h stays out of the
  shared headers: `graphics.cpp` includes `gs_privileged.h`, whose `GS_SET_*` macros clash with libgs.h's. The values several
  of the renderer's files write (the textured sprite, the depth test always passing, white, ...) are `renderer/gsvalues.h`'s,
  which only the renderer's .cpp files and the movie player include.
- **Floats**: a decimal literal single precision can't hold is `Rounded(...)` or a hex float (see "Float literals" below).

## What the build has to look out for

- **The memory budget**: the game's two pools take all but about 10 KB of what the executable leaves of the 32 MB: the disk
  manager's is 0x10A3D70 bytes and the heap manager's takes the rest (`Platform::Memory::PoolSpace`), like the retail game sized
  it. A pool that doesn't fit makes malloc fail, and the game uses the memory from address 0, the kernel's.
- **The stack is in the scratchpad**: the retail entry runs the main thread on the scratchpad's 16 KB, and the game needs it there.
  PS2SDK's loadfile, iopheap and fileio calls build what they send the IOP on their stack, which DMA doesn't reach there: the PS2
  side runs them on a stack in main memory (`src/platform/ps2/stack.h`).
- **Addresses that mean two things**: 0x3DB200 is the end of the last `.bss` array to `FUN_002941b0` and the heap's start to sbrk
  and the entry (`tools/fix_asm.py`). Data of a replaced function stays (its jump tables): its cases' labels become 0.
- **A file is linked whole or not at all**: splat put the functions spimdisasm found itself (`func_<address>`) into the file of
  the function before them, and replacing that function made them 0 too, vtable methods and a qsort comparator among them (loading
  a save sorted with a call to address 0). `tools/split.py` gives them files of their own, and `make_ld.py` stops when a replaced file
  has a label other than its cases'.
- **Arguments the asm passes on**: a function hands its callees the argument registers it got without setting them again, and a
  virtual call's slot may read them: `ReleaseChunkScenery` (FUN_001ecc60) gives the scenery's release its own `destroy`, which
  queues the scenery's destruction, and the C++ that left it out destroyed sceneries their chunks still had (on the first unload).
  `CallVirtual` and calls of asm need every argument the asm leaves in place written out.
- **What an argument points at**: AllocDmaTags reads the chain 0x18 past its argument: every caller hands it the whole set of
  buckets, not a bucket, and the translation that took it for a bucket moved a "next" pointer somewhere at random (Crash gone, a
  huge polygon over the beach). Read the offsets the asm uses from an argument (and what its callers pass) before typing it.
- **PS2SDK isn't Sony's SDK**: what the asm passes Sony's functions can mean something else to PS2SDK's. libmc's `mcChdir` always
  copies the current directory where it's told, Sony's skipped a null pointer, which the retail code passes (`Platform::MemoryCard`
  gives it a buffer).
- **The C library**: the link has the toolchain's newlib (`libc_nano`: `sprintf`, `snprintf` and string functions, which do
  what the game's did with every format and argument the program passes) and libgcc after PS2SDK's libkernel, whose `memcpy`,
  `memset`, `strlen` and `strncpy` are the game's own code; `ps2sdk.txt` gives the asm their retail names. Where newlib does
  otherwise, the game's are C++ under the retail names: `src/platform/ps2/libc.cpp` (`rand`, its state still in the old
  library's reentrancy block, `qsort`'s order of comparisons, `toupper`, `tolower` and `strncasecmp` by the game's casing table,
  `atexit`; `memmove` on PS2SDK's `memcpy`, as fast as the game's; `printf` on deci2's `kputs`, `puts` and `strlcpy` for
  PS2SDK), `malloc.cpp` (Sony's malloc, where the pools come from) and `libm.cpp` (`expf`). Newlib's `%ld` and `%p` take 32
  bits where Sony's took 64, and libc_nano prints no floats.
- **Functions that fall through**: splat splits hand-written code where it embeds data, so a "function" can be the tail
  of the one before it: libmpeg's FUN_002bf508 has no `jr ra` and runs through its lq mask (`D_002BF5B0`, whose words decode
  as harmless `dsra32 $zero`) into the `jr ra; nop` of FUN_002bf5c0. Nothing refers to that stub, and leaving it out of the
  link made FUN_002bf508 run on into `dmaRefImage`, which wrote DMA tags over all of memory at the logos. Before leaving a
  function out (`ps2sdk.txt`, `retired.txt`) or converting one alone, check that the one before it ends with a jump and its
  delay slot.
- **Alignment in `.text`**: libmpeg reads a mask in `.text` with `lq` (`D_002BF5B0`, aligned to 16 by `fix_asm.py`; the movies
  came out striped). `.vutext` is placed on its own, not with the code.
- **The R5900's floats**: GCC makes `1.0f / sqrt(x)` one `rsqrt.s`, which rounds unlike the retail `sqrt.s` and `div.s` (an
  empty asm statement keeps them apart, `InverseLength`), and fuses multiplies and adds into `madd.s` unless told not to
  (`-ffp-contract=off`). Matrix maths the asm does on VU0 stays VU0's (`include/game/math.h`'s `Vu*`): VU0 rounds otherwise.
- **Float literals**: GCC for the R5900 cuts decimal float literals down to single precision (rounding towards zero, as the
  R5900's FPU does), where retail's compiler rounded them to the nearest: `0.2f` is 0x3E4CCCCC in the C++ and 0x3E4CCCCD in the
  asm, and folding `1.0f / 255.0f` cuts down the same way. The difference is a bit of the last place, so it rarely shows: the box
  a step's triangles are gathered in grew by one bit less and a few pixels of the beach moved. Write a constant single precision
  can't hold as `Rounded(0.2)` (`include/common.h`, the nearest float to a double literal) or as a hex float.
- **Constants built by shifts**: work them out by running the asm's instructions (Python), never in your head: `ori 0x8000;
  dsll 24` is 0x80 << 32 (TEXA's TA1), read as 0x80000000 it took the depth copy's alpha away and the far scenery came out hazy.
  When the code reads right and a picture still differs, link the asm of one function at a time to find the function, then run
  the asm and the C++ into the same place of the chain every frame (keep `chain.next`, `bucket.last` and the word it points with,
  run one, copy what it wrote, put them back, run the other) and compare the words into debug globals `--dump` reads.
- **VU0 arithmetic in C++**: a C++ copy of a VU0 microprogram's steps doesn't give its results on the EE's FPU, even with the same
  operations in the same order: PCSX2 cuts the smaller operand of an FPU addition down to the bits the EE's adder keeps (no guard
  bits), VU0's additions keep them. The sine polynomial differed in the last bit now and then and moved a few pixels of the beach's
  water; it runs on VU0 again (`Platform::Math::SinCos`, the retail `vcallms 0xF0` sequence). Keep what VU0 computes on VU0.
- **Buffers the IOP writes**: SIF DMA ignores the low 4 bits of where it writes, and libmc's directory table has to be 64 byte
  aligned (`sceMcTblGetDir` is): `saves.cpp`'s table sat 8 bytes past a quadword, so every listing came in 8 bytes early and the
  game never found its saves (the references the render check had then were taken that way). Any new `.bss` moves such buffers.
- **Reads through null pointers**: the retail code reads through a null pointer now and then (an instance without an ID's
  designator reads the word at address 4, the head target's position reads a missing AgentRef1's place at address 8), which the
  PS2's low memory allows. `-fno-delete-null-pointer-checks` keeps GCC from taking a pointer it read through for one that isn't
  null and dropping the checks that follow; such reads go through an integer address in the C++ (`RetailPlaceOf`).
- **Words the asm never writes**: some packets leave words of a quadword as the buffer had them (a shader's entry, a corner's
  fourth word taken from the stack): the C++ leaves them too, rather than writing zeros the retail game never sent.

## Sony's SDK

In place: the kernel's system calls and helpers (PS2SDK's `_InitSys`, its DI/EI), SIF commands and RPC, iopcontrol, loadfile,
iopheap, fileio, libcdvd (`libxcdvd`), libpad (`libpadx`), libmc, libscf (the platform layer's language and local time, PS2SDK's
rom0_info), the device resets and libgraph's and libdma's calls the game makes (`src/platform/ps2/graphics.cpp`: the GS reset,
the waits for the vertical blank and the path to the GS, the vertical blank's callback, the DMA channels' registers, on PS2SDK's
system calls and register definitions; PS2SDK's own libgraph waits on the CSR and writes its other bits back) and libsdr's EE side
(`src/platform/ps2/sdr.cpp`, SDRDRV's commands sent by RPC the way PS2SDK's libsdr does, which PS2SDKs installed before 2026
don't have). GCC 2.9x's `__main` (the retail constructor list) is `src/gcc2.cpp`. Sony SDK code nothing reaches (libdev's VU
waits, the C library's scanf family, the kernel set-up's payloads only the replaced set-up pointed at) is left out through
`ps2sdk.txt`. libmpeg and libipu are the movie player's own C++ (`src/platform/ps2/movie/`: `mpeg.h` the decoder's state,
`mpeg.cpp` the interface, `mpegdemux.cpp` the PSS demultiplexer, `mpegoutput.cpp` the pictures' colour conversion,
`mpegdecode.cpp`, `mpegheaders.cpp`, `mpegbits.cpp` and `mpegmotion.cpp` the decoding, its hand-written MMI motion
compensation kept as its instructions, `ipu.cpp`), on PS2SDK's system calls and register definitions: PS2SDK's libmpeg has
another interface (no PSS demultiplexing, stream or error callbacks, RAW8 pictures) the player can't use without changing
what it does. The C library is newlib's and the game's own in C++ (see
"The C library" above), GCC's runtime the toolchain's libgcc.
