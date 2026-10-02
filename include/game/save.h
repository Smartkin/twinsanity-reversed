#pragma once

#include "common.h"

// The date a save keeps
struct SaveDate
{
    u8 day;
    u8 month;
    // Two digits
    u16 year;
    u8 hour;
    u8 minute;
    u8 second;
    // Bits 0 and 1 set: it's taken from the clock (again when it's refreshed)
    u8 bits;
};
CHECK_SIZE(SaveDate, 8);

extern "C"
{
    // The local date and time now
    void GetSaveDate(SaveDate* date) RETAIL(FUN_00192410);
}
