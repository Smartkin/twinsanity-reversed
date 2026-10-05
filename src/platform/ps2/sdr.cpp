#include "common.h"

#include <kernel.h>
#include <sifrpc.h>
#include <stdarg.h>
#include <stdio.h>

// The EE side of SDRDRV (Sony's libsdr): its commands go to the IOP's server by RPC with their arguments, the way PS2SDK's
// libsdr (ee/rpc/sdr) sends them too; the PS2SDKs installed before 2026 don't have that library. The retail game sends only
// plain commands, and nothing in it serves the callbacks it could register
namespace
{
constexpr s32 SdrServer = static_cast<s32>(0x80000701);

constexpr s32 SetEffectAttribute = 0x8130;
constexpr s32 GetEffectAttribute = 0x8140;
constexpr s32 SetTransferCallback = 0x8160;
constexpr s32 SetIrqCallback = 0x8170;
constexpr s32 SetEffectMode = 0x81A0;
constexpr s32 SetEffectModeParameters = 0x81B0;
constexpr s32 ProcessBatch = 0x81C0;
constexpr s32 ProcessBatchEx = 0x81D0;
constexpr s32 UserCommands = 0x9000;
constexpr u32 UserCommandCount = 0xF1;

// What a batch command sends: the first entry's halfword at 2 is the entry count, its word at 4 the extended value
struct Batch
{
    u16 function;
    u16 entry;
    u32 value;
};

constexpr u32 SendSize = 0x40;
constexpr u32 ReplySize = 0x10;
// The send buffer's arguments after its address
constexpr u32 RemoteArguments = 6;
// The callbacks' slots: the two cores' transfers', then the IRQ's
constexpr u32 IrqCallback = 2;
// The loops sceSdRemoteInit waits for the server's binding each time it isn't there
constexpr s32 BindWaitLoops = 10000;
}

extern "C"
{
    extern SifRpcClientData_t g_SdrClient RETAIL(D_003C7800);
    // The send buffer: its own address, then the command's arguments
    extern s32 g_SdrBuffer[SendSize / sizeof(s32)] RETAIL(D_003C77C0);
    // What a call that doesn't wait ends with, and the $gp it ran with
    extern SifRpcEndFunc_t g_SdrEndFunction RETAIL(D_002EA038);
    extern void* g_SdrEndFunctionGp RETAIL(D_002EA03C);
    // The callbacks registered for the transfers of the two cores and the IRQ, their arguments and $gp
    extern void* g_SdrCallbacks[IrqCallback + 1] RETAIL(D_002EA050);
    extern void* g_SdrCallbackArguments[IrqCallback + 1] RETAIL(D_002EA05C);
    extern void* g_SdrCallbackGps[IrqCallback + 1] RETAIL(D_002EA068);

    int sceSdRemoteInit();
    int sceSdRemote(int wait, int command, ...);
}

int sceSdRemoteInit()
{
    sceSifInitRpc(0);
    do
    {
        if (sceSifBindRpc(&g_SdrClient, SdrServer, 0) < 0)
        {
            printf("sceSdRemoteInit() RPC bind error!\n");
            return -1;
        }

        for (s32 wait = BindWaitLoops; wait != -1; wait--)
        {
            asm volatile("nop; nop; nop; nop");
        }
    } while (g_SdrClient.server == nullptr);

    g_SdrEndFunction = nullptr;
    g_SdrEndFunctionGp = nullptr;
    FlushCache(0);
    return 0;
}

int sceSdRemote(int wait, int command, ...)
{
    g_SdrBuffer[0] = reinterpret_cast<s32>(g_SdrBuffer);
    va_list arguments;
    va_start(arguments, command);
    for (u32 index = 1; index <= RemoteArguments; index++)
    {
        g_SdrBuffer[index] = va_arg(arguments, s32);
    }

    va_end(arguments);

    // Sony's call also ran an end function with the $gp kept for it: only its sceSdCallBack sets one, which the game never calls
    s32 mode = 0;
    SifRpcEndFunc_t ended = nullptr;
    if (wait == 0)
    {
        mode = SIF_RPC_M_NOWAIT;
        ended = g_SdrEndFunction;
    }

    void* gp;
    asm("move %0, $gp" : "=r"(gp));
    void* previous = nullptr;
    if (command == SetTransferCallback)
    {
        u32 core = g_SdrBuffer[1] != 0 ? 1 : 0;
        previous = g_SdrCallbacks[core];
        g_SdrCallbacks[core] = reinterpret_cast<void*>(g_SdrBuffer[2]);
        g_SdrCallbackArguments[core] = reinterpret_cast<void*>(g_SdrBuffer[3]);
        g_SdrCallbackGps[core] = gp;
    }
    else if (command == SetIrqCallback)
    {
        previous = g_SdrCallbacks[IrqCallback];
        g_SdrCallbacks[IrqCallback] = reinterpret_cast<void*>(g_SdrBuffer[1]);
        g_SdrCallbackArguments[IrqCallback] = reinterpret_cast<void*>(g_SdrBuffer[2]);
        g_SdrCallbackGps[IrqCallback] = gp;
    }

    s32* buffer = g_SdrBuffer;
    s32 result = 0;
    if (command == SetEffectAttribute || command == SetEffectMode || command == SetEffectModeParameters)
    {
        sceSifCallRpc(&g_SdrClient, command | buffer[1], mode, reinterpret_cast<void*>(buffer[2]), SendSize, buffer, SendSize,
                      ended, reinterpret_cast<void*>(buffer[0]));
        result = buffer[0];
    }
    else if (command == GetEffectAttribute)
    {
        void* attribute = reinterpret_cast<void*>(buffer[2]);
        sceSifCallRpc(&g_SdrClient, buffer[1] | GetEffectAttribute, mode, buffer, SendSize, attribute, SendSize, ended,
                      attribute);
    }
    else if (command == ProcessBatch || command == ProcessBatchEx)
    {
        auto* batch = reinterpret_cast<Batch*>(buffer[1]);
        batch[0].entry = static_cast<u16>(buffer[2]);
        if (command == ProcessBatchEx)
        {
            batch[0].value = buffer[5];
        }

        auto* reply = reinterpret_cast<s32*>(buffer[3]);
        u32 replySize = buffer[4];
        if (reply == nullptr)
        {
            reply = &result;
            replySize = sizeof(result);
        }

        sceSifCallRpc(&g_SdrClient, command, mode, batch, (buffer[2] + 1) * sizeof(Batch), reply, replySize, ended, buffer);
        result = *reply;
    }
    else if (static_cast<u32>(command - UserCommands) < UserCommandCount)
    {
        sceSifCallRpc(&g_SdrClient, command, mode, reinterpret_cast<void*>(buffer[1]), buffer[2], buffer, ReplySize, ended,
                      buffer);
        result = buffer[0];
    }
    else
    {
        sceSifCallRpc(&g_SdrClient, command, mode, buffer, SendSize, buffer, ReplySize, ended, buffer);
        result = buffer[0];
    }

    if (command == SetTransferCallback || command == SetIrqCallback)
    {
        return reinterpret_cast<s32>(previous);
    }

    return result;
}
