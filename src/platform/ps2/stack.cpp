#include "stack.h"

extern "C"
{
    alignas(64) u8 g_MainMemoryStack[0x4000];
}

// Switches only when the stack pointer is in the scratchpad (0x70000000)
asm(R"(
    .section .text.RunOnMainMemoryStack, "ax", @progbits
    .globl RunOnMainMemoryStack
    .type RunOnMainMemoryStack, @function
    .ent RunOnMainMemoryStack
    .set noreorder
RunOnMainMemoryStack:
    srl $8, $sp, 28
    li $9, 7
    beq $8, $9, 1f
    move $25, $4
    jr $25
    move $4, $5
1:
    move $8, $sp
    la $sp, g_MainMemoryStack + 0x4000 - 16
    sd $31, 0($sp)
    sd $8, 8($sp)
    jalr $25
    move $4, $5
    ld $31, 0($sp)
    ld $sp, 8($sp)
    jr $31
    nop
    .set reorder
    .end RunOnMainMemoryStack
    .size RunOnMainMemoryStack, . - RunOnMainMemoryStack
)");
