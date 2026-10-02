#pragma once

#include "common.h"

// The main thread's stack is in the scratchpad, which DMA doesn't reach at those addresses, and some of PS2SDK's calls send the
// IOP what they built on their stack or have it write there (libkernel's loadfile, iopheap and fileio calls). These run a call on
// a stack in main memory when they're called on the scratchpad's (the main thread's), and just run it otherwise. One stack: the
// main thread is the only one on the scratchpad
extern "C" s32 RunOnMainMemoryStack(s32 (*function)(void* context), void* context);

template <typename Function>
auto OnMainMemoryStack(Function function) -> decltype(function())
{
    using Result = decltype(function());
    struct Call
    {
        Function* function;
        Result result;
    };

    Call call{&function, Result()};
    RunOnMainMemoryStack(
        [](void* context) -> s32
        {
            Call* call = static_cast<Call*>(context);
            call->result = (*call->function)();
            return 0;
        },
        &call);
    return call.result;
}
