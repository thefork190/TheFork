#include <vector>
#include <string>

#include <SDL3/SDL_main.h>
#include <flecs.h>

#include <CLI.hpp>

#include "Modules/LifeCycledModule.h"

// Modules
#include "Modules/Low/Engine.h"
#include "Modules/Low/Inputs.h"
#include "Modules/Low/RHI.h"
#include "Modules/Low/Window.h"

#include "Modules/High/HelloTriangle/HelloTriangle.h"

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
    fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_GPU_CONFIG, "Assets/GPUCfg");
    fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_SHADER_BINARIES, "Assets/FSL/binary");

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
    std::vector<LifeCycledModule*> lowModules;
    std::vector<LifeCycledModule*> mediumModules;
    std::vector<LifeCycledModule*> highModules;
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

    // Setup ecs world and flecs explorer
    pApp->ecs = flecs::world(argc, argv);
    pApp->ecs.import<flecs::units>();
    pApp->ecs.import<flecs::stats>();
    pApp->ecs.set<flecs::Rest>({});


    // Import Low/Medium modules
    // TODO: which modules to load can be customized based on flags
    pApp->lowModules.push_back(pApp->ecs.import<Engine::module>().get_mut<Engine::module>());
    pApp->lowModules.push_back(pApp->ecs.import<Window::module>().get_mut<Window::module>());
    pApp->lowModules.push_back(pApp->ecs.import<RHI::module>().get_mut<RHI::module>());
    pApp->lowModules.push_back(pApp->ecs.import<Inputs::module>().get_mut<Inputs::module>());
    
    // Create the RHI (this might not always be needed depending on the app type)
    if (!RHI::CreateRHI(pApp->ecs))
    {
        return EXIT_FAILURE;
    }
    
    // Use the chosen module as the app name
    CLI::App cli{ "The Fork Engine" };
    argv = cli.ensure_utf8(argv);

    std::string moduleName = "Hello Triangle";
    cli.add_option("-a,--appmodule", moduleName, "The app (high level) module to use.");

    CLI11_PARSE(cli, argc, argv);

    // Kickstart the engine to activate the first systems
    Engine::KickstartEngine(pApp->ecs, &moduleName);

    // Now load high level modules
    // TODO:    High modules need to be data driven so we can know which to use/load (high modules represent "apps")
    //          Need to find a better way to the following.
    if ("Hello Triangle" == moduleName)
        pApp->highModules.push_back(pApp->ecs.import<HelloTriangle::module>().get_mut<HelloTriangle::module>());
    
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

    // Before progressing the world, check on the engine context state and act accordingly
    auto pEngineContext = pApp->ecs.has<Engine::Context>() ? pApp->ecs.get<Engine::Context>() : nullptr;
    if (pEngineContext)
    {
        if (pEngineContext->HasRequestedExit())
            pApp->quitApp = true;
    }

    pApp->ecs.progress();
    
    return pApp->quitApp;
}

void SDL_AppQuit(void* appstate) 
{
    AppState* pApp = (AppState*)appstate;

    if (pApp)
    {
        // Call OnExit on all modules (starting from high level modules)
        for (auto& pModule : pApp->highModules)
            pModule->OnExit(pApp->ecs);

        for (auto& pModule : pApp->mediumModules)
            pModule->OnExit(pApp->ecs);

        for (auto& pModule : pApp->lowModules)
            pModule->OnExit(pApp->ecs);

        tf_delete(pApp);
    }

    SDL_Quit();
    LOGF(eINFO, "Application quit successfully!");

    ExitTheForge();
}
