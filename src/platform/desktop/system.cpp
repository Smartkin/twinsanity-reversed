#include "platform/system.h"

#include "window.h"

#include <cstdlib>
#include <ctime>

// The desktop's side of Platform::System: nothing to start, English, the computer's local time, the window closed at the exit
void Platform::System::Initialise()
{
}

void Platform::System::StartServices()
{
}

void Platform::System::Exit(s32 status)
{
    DesktopWindow::Close();
    std::exit(status);
}

Platform::System::ConsoleLanguage Platform::System::Language()
{
    return LanguageEnglish;
}

Platform::System::DateTime Platform::System::LocalTime()
{
    std::time_t now = std::time(nullptr);
    std::tm local = *std::localtime(&now);
    DateTime time;
    time.year = static_cast<u16>(local.tm_year + 1900);
    time.month = static_cast<u8>(local.tm_mon + 1);
    time.day = static_cast<u8>(local.tm_mday);
    time.hour = static_cast<u8>(local.tm_hour);
    time.minute = static_cast<u8>(local.tm_min);
    time.second = static_cast<u8>(local.tm_sec);
    return time;
}
