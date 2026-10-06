#include "common.h"

#include <cstdlib>

// The C library's desktop side: abort and the retail crt0's exit end the program
extern "C"
{
    [[noreturn]] void RetailExit(s32 status) RETAIL(FUN_001000e0);
    [[noreturn]] void Abort() RETAIL(FUN_002c79d8);
}

void Abort()
{
    RetailExit(1);
}

void RetailExit(s32)
{
    std::exit(0);
}
