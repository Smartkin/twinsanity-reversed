#include "game/save.h"

#include "platform/system.h"
#include "retail/libc.h"

extern "C"
{
    SaveDate* ConstructSaveDate(SaveDate* date)
    {
        return static_cast<SaveDate*>(RetailLibc::MemorySet(date, 0, sizeof(SaveDate)));
    }

    void GetSaveDate(SaveDate* date)
    {
        Platform::System::DateTime now = Platform::System::LocalTime();
        date->bits |= 3;
        date->day = now.day;
        date->second = now.second;
        date->month = now.month;
        date->year = now.year % 100;
        date->hour = now.hour;
        date->minute = now.minute;
    }
}
