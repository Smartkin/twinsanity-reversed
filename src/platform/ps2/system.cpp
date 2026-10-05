#include "platform/system.h"
#include "platform/disc.h"
#include "platform/files.h"
#include "platform/graphics.h"
#include "platform/io.h"
#include "memorycard.h"
#include "platform/pads.h"
#include "platform/sound.h"

#include <kernel.h>
#include <libcdvd.h>
#include <osd_config.h>
#include <rom0_info.h>

namespace
{
// Where the I/O processor's modules are on the disc
constexpr const char* SystemFolder = "cdrom0:\\CRASH6\\SYS\\";
// The SDK's newer modules the I/O processor restarts with
constexpr const char* ModuleImage = "IOPRP300.IMG";
// The drivers of the pads, the memory cards, the sound processor and the streams, loaded after the restart
constexpr const char* Drivers[] = {"SIO2MAN.IRX", "PADMAN.IRX", "MCMAN.IRX", "MCSERV.IRX",
                                   "LIBSD.IRX",   "SDRDRV.IRX", "STREAM.IRX", "CDVDSTM.IRX"};

constexpr u32 MaxPath = 64;

void SystemPath(char (&path)[MaxPath], const char* file)
{
    const char* parts[] = {SystemFolder, file};
    u32 length = 0;
    for (const char* part : parts)
    {
        while (*part != '\0' && length < MaxPath - 1)
        {
            path[length++] = *part++;
        }
    }

    path[length] = '\0';
}

void StartDisc()
{
    Platform::Io::Initialise();
    Platform::Disc::Initialise();
    Platform::Disc::SetMedia(Platform::Disc::Media::Dvd);
    Platform::Files::Reset();
    Platform::Disc::WaitReady();
}
}

// The retail Main's start-up (and InitDisk's): the I/O processor restarts with the disc's IOPRP image, and the disc drive starts
// again after it
void Platform::System::Initialise()
{
    Io::Initialise();
    Graphics::ResetDevices();
    StartDisc();
    char image[MaxPath];
    SystemPath(image, ModuleImage);
    Io::Restart(image);
    StartDisc();
}

void Platform::System::StartServices()
{
    for (const char* driver : Drivers)
    {
        char path[MaxPath];
        SystemPath(path, driver);
        Io::LoadDriver(path);
    }

    MemoryCard::Initialise();
    Pads::Initialise();
    Io::InitialiseHeap();
    Sound::InitialiseRemote();
}

extern "C"
{
    // libscf's settings of a development kit (a T10K), which has no OSD settings of its own
    extern s16 g_DevelopmentKitTimeZone RETAIL(D_002E71B0);
    extern u8 g_DevelopmentKitLanguage RETAIL(D_002E71B4);
    extern u8 g_DevelopmentKitSummerTime RETAIL(D_002E71B6);
}

namespace
{
// libscf's local time: the console's clock keeps Japan's time, the OSD settings say how many minutes the local time is from
// UTC and whether it's summer time, which puts it an hour on. Settings before version 1 had no time zone
constexpr s32 JapanTimeZone = 540;
constexpr s32 SummerTimeMinutes = 60;

// The OSD settings' first word (PS2SDK's ConfigParam): the time zone's offset is its top 11 bits, signed
union OsdSettingsWord
{
    u32 value;
    struct
    {
        u32 unused0 : 21;
        s32 timeZoneOffset : 11;
    };
};
CHECK_SIZE(OsdSettingsWord, 4);

// The second settings' byte at 1 (PS2SDK's Config2Param's)
union OsdTimeSettings
{
    u8 value;
    struct
    {
        u8 unused0 : 4;
        u8 summerTime : 1;
        u8 unused5 : 3;
    };
};
CHECK_SIZE(OsdTimeSettings, 1);
constexpr s32 TimeSettingsOffset = 1;

s32 TimeZone()
{
    if (IsT10K())
    {
        return g_DevelopmentKitTimeZone;
    }

    ConfigParam config;
    GetOsdConfigParam(&config);
    if (config.version == 0)
    {
        return JapanTimeZone;
    }

    OsdSettingsWord settings;
    __builtin_memcpy(&settings.value, &config, sizeof(settings.value));
    return settings.timeZoneOffset;
}

bool SummerTime()
{
    if (IsT10K())
    {
        return g_DevelopmentKitSummerTime != 0;
    }

    ConfigParam config;
    GetOsdConfigParam(&config);
    if (config.version == 0)
    {
        return false;
    }

    OsdTimeSettings settings;
    GetOsdConfigParam2(&settings.value, sizeof(settings.value), TimeSettingsOffset);
    return settings.summerTime != 0;
}

// A tens digit counts 16 in BCD, 6 too many
u8 FromBcd(u8 value)
{
    return static_cast<u8>(value - (value >> 4) * 6);
}

// libscf steps the date a day at a time, on years of two digits (every fourth one a leap year)
u8 DaysIn(u8 month, u8 year)
{
    constexpr u8 Days[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month == 2 && (year & 3) == 0)
    {
        return 29;
    }

    return month >= 1 && month <= 12 ? Days[month - 1] : 31;
}

void NextHour(Platform::System::DateTime& time)
{
    if (++time.hour != 24)
    {
        return;
    }

    time.hour = 0;
    time.day++;
    if (DaysIn(time.month, static_cast<u8>(time.year)) >= time.day)
    {
        return;
    }

    time.day = 1;
    if (++time.month == 13)
    {
        time.year = time.year == 99 ? 0 : time.year + 1;
        time.month = 1;
    }
}

void PreviousHour(Platform::System::DateTime& time)
{
    if (time.hour != 0)
    {
        time.hour--;
        return;
    }

    time.hour = 23;
    u8 year = static_cast<u8>(time.year);
    if (--time.day != 0)
    {
        return;
    }

    if (--time.month == 0)
    {
        time.year = time.year == 0 ? 99 : time.year - 1;
        time.month = 12;
    }

    time.day = DaysIn(time.month, year);
}
}

Platform::System::ConsoleLanguage Platform::System::Language()
{
    // libscf's sceScfGetLanguage: the OSD settings before version 1 only knew Japanese and English
    ConfigParam config;
    GetOsdConfigParam(&config);
    if (IsT10K())
    {
        return static_cast<ConsoleLanguage>(g_DevelopmentKitLanguage);
    }

    GetOsdConfigParam(&config);
    if (config.version != 0)
    {
        return static_cast<ConsoleLanguage>(config.language);
    }

    return static_cast<ConsoleLanguage>(config.japLanguage);
}

Platform::System::DateTime Platform::System::LocalTime()
{
    sceCdCLOCK clock;
    sceCdReadClock(&clock);
    DateTime time;
    time.year = FromBcd(clock.year);
    time.month = FromBcd(clock.month);
    time.day = FromBcd(clock.day);
    time.hour = FromBcd(clock.hour);
    time.second = FromBcd(clock.second);
    s32 minute = FromBcd(clock.minute) + TimeZone() + (SummerTime() ? SummerTimeMinutes : 0) - JapanTimeZone;
    while (minute < 0)
    {
        minute += 60;
        PreviousHour(time);
    }

    while (minute >= 60)
    {
        NextHour(time);
        minute -= 60;
    }

    time.minute = static_cast<u8>(minute);
    // The clock's year has two digits
    time.year += 2000;
    return time;
}
