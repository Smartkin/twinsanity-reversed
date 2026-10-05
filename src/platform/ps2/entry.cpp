#include "common.h"
#include "platform/system.h"
#include "stack.h"

#include <kernel.h>
#include <rom0_info.h>

// The program's entry, the retail one's: the main thread runs on the scratchpad (its 16 KB filled with 0xA1B2C3D4 to see how
// deep the stack went), which the game relies on (it hangs at start-up with the stack in main memory). PS2SDK's calls that DMA
// from or to their stack run on another one (stack.h)
extern "C"
{
    extern char _end[];

    // Where the kernel puts the arguments the program was started with
    extern s32 MainARGC;
    extern char* MainARGV[];

    int Main(u32 argc, char** argv);

    // Sony's SystemInit set up its kernel helpers here: PS2SDK's (alarms, the timer, threads, the ExecPS2 and TLB patches) take
    // their place. PS2SDK reads the console's ROM version through the IOP from its stack, the first time something asks (its
    // GetOsdConfigParam does, which the game calls on this stack): it's asked here, on one in main memory, and kept
    [[noreturn]] void StartProgram()
    {
        SetupHeap(_end, -1);
        OnMainMemoryStack(
            []
            {
                _InitSys();
                return IsT10K();
            });
        FlushCache(0);
        EI();
        Platform::System::Exit(Main(MainARGC, MainARGV));
    }

    // The retail crt0's exit (FUN_001000e0) still calls it
    [[noreturn]] void ProgramExit(int status)
    {
        Platform::System::Exit(status);
    }
}

void Platform::System::Exit(s32 status)
{
    TerminateLibrary();
    ::Exit(status);
}

// Clears .sbss and .bss, makes the main thread (SetupThread: no stack of its own in main memory, the heap may reach the top) and
// moves its stack to the scratchpad, filling it with a pattern first
asm(R"(
    .section .text.entry, "ax", @progbits
    .globl entry
    .type entry, @function
    .ent entry
    .set noreorder
    .set noat
    .equ SetupThreadSyscall, 60
    .equ ScratchpadStart, 0x70000000
    .equ ScratchpadSize, 0x4000
    .equ StackFill, 0xA1B2C3D4
entry:
    la $2, _fbss
    la $3, _end
1:
    sltu $1, $2, $3
    beq $1, $0, 2f
    nop
    sq $0, 0($2)
    b 1b
    addiu $2, $2, 16
2:
    la $4, _gp
    li $5, -1
    move $6, $0
    la $7, MainARGC
    la $8, ExitThread
    move $gp, $4
    li $3, SetupThreadSyscall
    syscall
    li $sp, ScratchpadStart + ScratchpadSize
    li $8, ScratchpadStart
    li $9, ScratchpadSize
    li $10, StackFill
3:
    sw $10, 0($8)
    addiu $9, $9, -4
    bgtz $9, 3b
    addiu $8, $8, 4
    j StartProgram
    nop
    .set at
    .set reorder
    .end entry
    .size entry, . - entry
)");
