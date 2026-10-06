#include "common.h"
#include "platform/system.h"

// The desktop's entry: the game's Main with the program's arguments
extern "C" int Main(u32 argc, char** argv);

int main(int argc, char** argv)
{
    Platform::System::Exit(Main(static_cast<u32>(argc), argv));
}
