#pragma once

// The retail executable's data (src/data/, and the PS2 side's VU microcode) is defined in the retail order, each object under
// its retail name: the game's code reads some tables past the object it names into the next ones, so the objects stay where
// the retail executable had them, one after the other in their section. Their files are compiled in the order they're written
// (GCC's -fno-toplevel-reorder, Clang keeps it), with the objects nothing names (used), the PS2's assembler doesn't pad their
// sections (-no-pad-sections) and the PS2's linker script puts them first in their sections.
#if defined(_EE)
#define RETAIL_DATA(name, alignment) __attribute__((section(name), aligned(alignment), used))
#else
// Elsewhere every section of it goes in the host's data (.data.retail.rodata and so on), its files linked first in the PS2's order
// of the sections (configure.py): one block the game's code reads across (the copy protection from .sbss into .bss) and writes
// anywhere in (ParseArguments into the launch arguments in .rodata), as on the PS2
#define RETAIL_DATA(name, alignment) __attribute__((section(".data.retail" name), aligned(alignment), used))
#endif
