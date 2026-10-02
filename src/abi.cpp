#include "abi.h"

// Abi::CallEabi's call: $a0-$a7 from the first array, $f12-$f19 from the second, and the result left in $v0 and $f0
asm(R"(
    .section .text.CallEabiTrampoline, "ax", @progbits
    .globl CallEabiTrampoline
    .type CallEabiTrampoline, @function
    .set push
    .set noreorder
CallEabiTrampoline:
    addiu $sp, $sp, -16
    sd $31, 0($sp)
    move $25, $4
    move $24, $5
    lwc1 $f12, 0($6)
    lwc1 $f13, 4($6)
    lwc1 $f14, 8($6)
    lwc1 $f15, 12($6)
    lwc1 $f16, 16($6)
    lwc1 $f17, 20($6)
    lwc1 $f18, 24($6)
    lwc1 $f19, 28($6)
    ld $4, 0($24)
    ld $5, 8($24)
    ld $6, 16($24)
    ld $7, 24($24)
    ld $8, 32($24)
    ld $9, 40($24)
    ld $10, 48($24)
    jalr $25
    ld $11, 56($24)
    ld $31, 0($sp)
    jr $31
    addiu $sp, $sp, 16
    .set pop
    .size CallEabiTrampoline, . - CallEabiTrampoline
)");
