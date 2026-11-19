#include <windows.h>

#include "KnWindow.h"
#include "Env.h"

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    Env::init();

    dui::Application app;
    KnWindow window;
    return app.Run(window);
}
