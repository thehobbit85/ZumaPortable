//
// Zuma Deluxe - Android/desktop entry point.
//
// Upstream (Windows) entry was WinMain in Main.cpp; SDL2main provides WinMain on
// Windows and SDLActivity drives this on Android, so a plain main() is correct
// for every target.
//
#include <SDL_main.h>

#include "Zuma/Zuma_Prefix.pch"
#include "Zuma/CircleShootApp.h"

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    Sexy::CircleShootApp* anApp = new Sexy::CircleShootApp();
    anApp->Init();
    anApp->Start();
    anApp->Shutdown();
    delete anApp;

    return 0;
}
