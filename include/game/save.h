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
    // Both of Valid's bits set: it holds a date (the clock's, taken when a save is made and again when it's refreshed, or a card
    // file's), which a save slot's summary does while the slot holds a save
    u8 bits;

    static constexpr u8 Valid = 3;
};
CHECK_SIZE(SaveDate, 8);

extern "C"
{
    // A date made empty (every field 0), and the local date and time now
    SaveDate* ConstructSaveDate(SaveDate* date) RETAIL(FUN_00181c00);
    void GetSaveDate(SaveDate* date) RETAIL(FUN_00192410);
}
