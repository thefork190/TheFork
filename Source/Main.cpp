#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <cmath>

// TF 
#include <ILog.h>
#include <IMemory.h>
#include <IFileSystem.h>


#define APP_NAME "The Fork"

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

struct AppContext 
{
    SDL_Window* pWindow;
    bool quitApp = false;
};

int SDL_Fail()
{
    LOGF(eERROR, "Error %s", SDL_GetError());
    return -1;
}

int SDL_AppInit(void** appstate, int argc, char* argv[]) 
{
    if (!InitTheForge())
    {
        return EXIT_FAILURE;
    }
    
    if (SDL_Init(SDL_INIT_VIDEO))
    {
        return SDL_Fail();
    }
    
    SDL_Window* pWin = SDL_CreateWindow(APP_NAME, 1920, 1080, SDL_WINDOW_RESIZABLE | SDL_WINDOW_VULKAN);
    if (!pWin)
    {
        return SDL_Fail();
    }

    SDL_ShowWindow(pWin);
    {
        int width, height, bbwidth, bbheight;
        SDL_GetWindowSize(pWin, &width, &height);
        SDL_GetWindowSizeInPixels(pWin, &bbwidth, &bbheight);
        LOGF(eINFO, "Window size: %ix%i", width, height);
        LOGF(eINFO, "Backbuffer size: %ix%i", bbwidth, bbheight);
        if (width != bbwidth)
        {
            LOGF(eINFO, "High dpi detected.");
        }
    }

    *appstate = tf_new(AppContext);
    ((AppContext*)(*appstate))->pWindow = pWin;
    
    LOGF(eINFO, "SDL_AppInit returns success.");

    return 0;
}

int SDL_AppEvent(void *appstate, const SDL_Event* event) {
    AppContext* app = (AppContext*)appstate;
    
    if (event->type == SDL_EVENT_QUIT) 
    {
        app->quitApp = SDL_TRUE;
    }

    return 0;
}

int SDL_AppIterate(void *appstate) 
{
    AppContext* app = (AppContext*)appstate;
    
    return app->quitApp;
}

void SDL_AppQuit(void* appstate) 
{
    AppContext* app = (AppContext*)appstate;
    if (app) 
    {
        SDL_DestroyWindow(app->pWindow);
        tf_delete(app);
    }

    SDL_Quit();
    LOGF(eINFO, "Application quit successfully!");

    exitFileSystem();
    exitLog();
    exitMemAlloc();
}
