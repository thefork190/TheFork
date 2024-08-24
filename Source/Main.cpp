#include <SDL3/SDL_main.h>
#include <flecs.h>

// Modules
#include "Modules/Low/Engine.h"
#include "Modules/Low/Window.h"
#include "Modules/Low/RHI.h"

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
    pApp->ecs.import<Window::module>();
    pApp->ecs.import<RHI::module>();

    // Create the RHI (this might not always be needed depending on the app type)
    if (!RHI::CreateRHI(pApp->ecs))
    {
        return EXIT_FAILURE;
    }

    // Kickstart the engine to activate the first systems
    Engine::KickstartEngine(pApp->ecs);
    
     
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
        tf_delete(pApp);
    }

    SDL_Quit();
    LOGF(eINFO, "Application quit successfully!");

    ExitTheForge();
}
