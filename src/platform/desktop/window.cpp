#include "window.h"

#if defined(DESKTOP_SDL2)
#include <cstdio>

// The program's own main, not SDL's
#define SDL_MAIN_HANDLED
#include <SDL.h>

namespace
{
// A TV's 4:3 at 720 lines to start with
constexpr s32 WindowHeight = 720;
constexpr s32 WindowWidth = WindowHeight * 4 / 3;

bool g_Started = false;
SDL_Window* g_Window = nullptr;
SDL_Renderer* g_Renderer = nullptr;
}

// Without a display (or SDL's video) the game goes on without a window
void DesktopWindow::Open()
{
    if (g_Started)
    {
        return;
    }

    SDL_SetMainReady();
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
    {
        std::fprintf(stderr, "No window: %s\n", SDL_GetError());
        return;
    }

    g_Started = true;
    g_Window = SDL_CreateWindow("Crash Twinsanity", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WindowWidth, WindowHeight,
                                SDL_WINDOW_RESIZABLE);
    if (g_Window == nullptr)
    {
        std::fprintf(stderr, "No window: %s\n", SDL_GetError());
        return;
    }

    // The vertical blank is the game's to wait for (Platform::Graphics::WaitVSync), presenting doesn't
    g_Renderer = SDL_CreateRenderer(g_Window, -1, 0);
    if (g_Renderer == nullptr)
    {
        std::fprintf(stderr, "Nothing shown in the window: %s\n", SDL_GetError());
    }
}

bool DesktopWindow::HandleEvents()
{
    bool closed = false;
    SDL_Event event;
    while (SDL_PollEvent(&event) != 0)
    {
        if (event.type == SDL_QUIT)
        {
            closed = true;
        }
    }

    return closed;
}

void DesktopWindow::Show(u8 red, u8 green, u8 blue)
{
    if (g_Renderer == nullptr)
    {
        return;
    }

    SDL_SetRenderDrawColor(g_Renderer, red, green, blue, SDL_ALPHA_OPAQUE);
    SDL_RenderClear(g_Renderer);
    SDL_RenderPresent(g_Renderer);
}

void DesktopWindow::Close()
{
    if (!g_Started)
    {
        return;
    }

    if (g_Renderer != nullptr)
    {
        SDL_DestroyRenderer(g_Renderer);
        g_Renderer = nullptr;
    }

    if (g_Window != nullptr)
    {
        SDL_DestroyWindow(g_Window);
        g_Window = nullptr;
    }

    SDL_Quit();
    g_Started = false;
}
#else
void DesktopWindow::Open()
{
}

bool DesktopWindow::HandleEvents()
{
    return false;
}

void DesktopWindow::Show(u8, u8, u8)
{
}

void DesktopWindow::Close()
{
}
#endif
