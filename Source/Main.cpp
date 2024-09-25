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

#include "Modules/Medium/FontRendering.h"
#include "Modules/Medium/Imgui/UI.h"

#include "Modules/High/FlappyClone/FlappyClone.h"
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

#ifdef __ANDROID__
    ASSERT(0 != SDL_GetAndroidExternalStorageState());
    fsDesc.pResourceMounts[RM_DEBUG] = SDL_GetAndroidExternalStoragePath();
    ASSERT(fsDesc.pResourceMounts[RM_DEBUG]);
#endif

    if (!initFileSystem(&fsDesc))
        return false;

    fsSetPathForResourceDir(pSystemFileIO, RM_DEBUG, RD_LOG, "");
    fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_FONTS, "Assets/Fonts");
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

SDL_AppResult SDL_Fail()
{
    LOGF(eERROR, "Error %s", SDL_GetError());
    return SDL_APP_FAILURE;
}

SDL_AppResult SDL_AppInit(void** appstate, int argc, char* argv[])
{
    // Init SDL.  Many systems will rely on SDL being initialized.
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        return SDL_Fail();
    }

    // Use TF for rendering (it still needs to init its internal OS related subsystems)
    if (!InitTheForge())
    {
        return SDL_APP_FAILURE;
    }

    *appstate = tf_new(AppState);
    AppState* pApp = reinterpret_cast<AppState*>(*appstate);

    // Setup ecs world and flecs explorer
    pApp->ecs = flecs::world(argc, argv);
    pApp->ecs.import<flecs::units>();
    pApp->ecs.import<flecs::stats>();
    pApp->ecs.set<flecs::Rest>({});


    // Import Low/Medium modules
    pApp->lowModules.push_back(pApp->ecs.import<Engine::module>().get_mut<Engine::module>());
    pApp->lowModules.push_back(pApp->ecs.import<Window::module>().get_mut<Window::module>());
    pApp->lowModules.push_back(pApp->ecs.import<RHI::module>().get_mut<RHI::module>());
    pApp->lowModules.push_back(pApp->ecs.import<Inputs::module>().get_mut<Inputs::module>());

    pApp->lowModules.push_back(pApp->ecs.import<FontRendering::module>().get_mut<FontRendering::module>());
    pApp->lowModules.push_back(pApp->ecs.import<UI::module>().get_mut<UI::module>());

    
    // Create the RHI (this might not always be needed depending on the app type)
    if (!RHI::CreateRHI(pApp->ecs))
    {
        return SDL_APP_FAILURE;
    }
    
    // Use the chosen module as the app name
    CLI::App cli{ "The Fork Engine" };
    argv = cli.ensure_utf8(argv);

    std::string moduleName = "Hello Triangle";
    cli.add_option("-a,--appmodule", moduleName, "The app (high level) module to use.");

    cli.parse(argc, argv);

    // Kickstart the engine to activate the first systems
    Engine::KickstartEngine(pApp->ecs, &moduleName);

    // Now load high level modules
    // TODO:    High modules need to be data driven so we can know which to use/load (high modules represent "apps")
    //          Need to find a better way to the following.
    if ("Hello Triangle" == moduleName)
        pApp->highModules.push_back(pApp->ecs.import<HelloTriangle::module>().get_mut<HelloTriangle::module>());
    else if("Flappy Clone" == moduleName)
        pApp->highModules.push_back(pApp->ecs.import<FlappyClone::module>().get_mut<FlappyClone::module>());
    
    LOGF(eINFO, "SDL_AppInit returns success.");

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event* event)
{
    AppState* pApp = (AppState*)appstate;
    
    if (event->type == SDL_EVENT_QUIT) 
    {
        pApp->quitApp = true;
    }

    UI::ForwardEvent(pApp->ecs, event);

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *appstate)
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
    
    return pApp->quitApp ? SDL_APP_SUCCESS : SDL_APP_CONTINUE;
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
