#pragma once

// The retail executable's data (src/data/, and the PS2 side's VU microcode) is defined in the retail order, each object under
// its retail name: the game's code reads some tables past the object it names into the next ones, so the objects stay where
// the retail executable had them, one after the other in their section. Their files are compiled in the order they're written
// (GCC's -fno-toplevel-reorder, Clang keeps it), with the objects nothing names (used), the PS2's assembler doesn't pad their
// sections (-no-pad-sections) and the PS2's linker script puts them first in their sections.
//
// Elsewhere it's one block in the host's data, its files linked first in the PS2's order of the sections (configure.py): the
// game's code reads across sections (the copy protection from .sbss into .bss) and writes anywhere in it (ParseArguments into
// the launch arguments in .rodata), as on the PS2. ELF linkers put the .data.retail sections into .data in the files' order,
// Windows' linkers put the .data$ sections into .data ordered by their names and then by the files', so there they share one
#if defined(_EE)
#define RETAIL_DATA(name, alignment) __attribute__((section(name), aligned(alignment), used))
#elif defined(_WIN32)
#define RETAIL_DATA(name, alignment) __attribute__((section(".data$retail"), aligned(alignment), used))
#else
#define RETAIL_DATA(name, alignment) __attribute__((section(".data.retail" name), aligned(alignment), used))
#endif
