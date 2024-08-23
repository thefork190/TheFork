#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <cmath>

#include <flecs.h>

// Modules
#include "Modules/Engine.h"

// TF 
#include <ILog.h>
#include <IFileSystem.h>
#include <IGraphics.h>
#include <IMemory.h> // Make sure this is last
//

static bool InitTheForge()
{
    FileSystemInitDesc fsDesc = {};
    fsDesc.pAppName = APP_NAME;
    if (!initFileSystem(&fsDesc))
        return false;

    fsSetPathForResourceDir(pSystemFileIO, RM_DEBUG, RD_LOG, "");

    initMemAlloc(APP_NAME);
    initLog(APP_NAME, DEFAULT_LOG_LEVEL);

    return true;
}

static void ExitTheForge()
{
    exitFileSystem();
    exitLog();
    exitMemAlloc();
}

struct AppState 
{
    //SDL_Window* pWindow;
    bool quitApp = false;
    flecs::world ecs;
};

int SDL_Fail()
{
    LOGF(eERROR, "Error %s", SDL_GetError());
    return -1;
}

int SDL_AppInit(void** appstate, int argc, char* argv[]) 
{
    // Use TF for rendering (it still needs to init its internal OS related subsystems)
    if (!InitTheForge())
    {
        return EXIT_FAILURE;
    }
    
    // Init SDL.  Many systems will rely on SDL being initialized.
    if (SDL_Init(SDL_INIT_VIDEO))
    {
        return SDL_Fail();
    }

    *appstate = tf_new(AppState);
    AppState* pApp = reinterpret_cast<AppState*>(*appstate);

    // Import modules
    // TODO: which modules to load can be customized based on flags
    pApp->ecs.import<Engine::module>();
    
    /*SDL_Window* pWin = SDL_CreateWindow(APP_NAME, 1920, 1080, SDL_WINDOW_RESIZABLE | SDL_WINDOW_VULKAN);
    if (!pWin)
    {
        return SDL_Fail();
    }*/

    //SDL_ShowWindow(pWin);
   /* {
        int width, height, bbwidth, bbheight;
        SDL_GetWindowSize(pWin, &width, &height);
        SDL_GetWindowSizeInPixels(pWin, &bbwidth, &bbheight);
        LOGF(eINFO, "Window size: %ix%i", width, height);
        LOGF(eINFO, "Backbuffer size: %ix%i", bbwidth, bbheight);
        if (width != bbwidth)
        {
            LOGF(eINFO, "High dpi detected.");
        }
    }*/
    
    
    //((AppState*)(*appstate))->pWindow = pWin;
    
    LOGF(eINFO, "SDL_AppInit returns success.");

    return 0;
}

int SDL_AppEvent(void *appstate, const SDL_Event* event) 
{
    AppState* pApp = (AppState*)appstate;
    
    if (event->type == SDL_EVENT_QUIT) 
    {
        pApp->quitApp = SDL_TRUE;
    }

    return 0;
}

int SDL_AppIterate(void *appstate) 
{
    AppState* pApp = (AppState*)appstate;
    pApp->ecs.progress();
    
    return pApp->quitApp;
}

void SDL_AppQuit(void* appstate) 
{
    AppState* pApp = (AppState*)appstate;
    if (pApp)
    {
        //SDL_DestroyWindow(app->pWindow);
        tf_delete(pApp);
    }

    SDL_Quit();
    LOGF(eINFO, "Application quit successfully!");

    ExitTheForge();
}
