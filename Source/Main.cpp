#include <vector>
#include <string>

#ifdef __ANDROID__
#include <jni.h>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <SDL3/SDL_system.h>
#endif

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

#include "Modules/High/AppModuleLauncher/AppModuleLauncher.h"

// TF 
#include <ILog.h>
#include <IFileSystem.h>
#include <IGraphics.h>
#include <IMemory.h> // Make sure this is last
//

#ifdef __ANDROID__
AAssetManager* GetAssetManager()
{
    // Get the JNI environment
    JNIEnv* env = (JNIEnv*)SDL_GetAndroidJNIEnv();

    // Get the Android activity
    jobject activity = (jobject)SDL_GetAndroidActivity();

    // Find the class for the Android activity
    jclass activityClass = env->GetObjectClass(activity);

    // Get the method ID for "getAssets", which returns the AAssetManager
    jmethodID getAssetsMethod = env->GetMethodID(activityClass, "getAssets", "()Landroid/content/res/AssetManager;");

    // Call the getAssets method on the activity to get the AssetManager
    jobject assetManager = env->CallObjectMethod(activity, getAssetsMethod);

    // Convert the jobject to an AAssetManager
    AAssetManager* aAssetManager = AAssetManager_fromJava(env, assetManager);

    // Clean up local references
    env->DeleteLocalRef(activityClass);
    env->DeleteLocalRef(assetManager);

    return aAssetManager;
}
#endif

static bool InitTheForge()
{
    FileSystemInitDesc fsDesc = {};
    fsDesc.pAppName = APP_NAME;

#ifdef __ANDROID__
    ASSERT(0 != SDL_GetAndroidExternalStorageState());
    fsDesc.pResourceMounts[RM_DEBUG] = SDL_GetAndroidExternalStoragePath();
    ASSERT(fsDesc.pResourceMounts[RM_DEBUG]);

    fsDesc.pPlatformData = reinterpret_cast<void*>(GetAssetManager());
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
    bool pauseApp = false;
    flecs::world ecs;
    std::vector<LifeCycledModule*> lowModules;
    std::vector<LifeCycledModule*> mediumModules;
    LifeCycledModule* pAppLauncherModule = nullptr;
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

    std::string moduleName = "";
    cli.add_option("-a,--appmodule", moduleName, "The app (high level) module to use.");

    cli.parse(argc, argv);

    // Kickstart the engine to activate the first systems
    Engine::KickstartEngine(pApp->ecs);

    // Setup the app launcher module that will handle launching the proper app
    AppModuleLauncher::module::SetAppModuleToStart(moduleName);
    pApp->pAppLauncherModule = pApp->ecs.import<AppModuleLauncher::module>().get_mut<AppModuleLauncher::module>();

    LOGF(eINFO, "SDL_AppInit returns success.");

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event* event)
{
    AppState* pApp = (AppState*)appstate;
    
    switch (event->type)
    {
        case SDL_EVENT_QUIT:
        {
            pApp->quitApp = true;
            break;
        }
        case SDL_EVENT_TERMINATING:
        {
            pApp->quitApp = true;
            break;
        }
        case SDL_EVENT_LOW_MEMORY:
        {
            LOGF(eINFO, "[HANDLE ME!]: low memory!");
            break;
        }
        case SDL_EVENT_WILL_ENTER_BACKGROUND:
        case SDL_EVENT_DID_ENTER_BACKGROUND:
        case SDL_EVENT_WILL_ENTER_FOREGROUND:
        case SDL_EVENT_WINDOW_MINIMIZED:
        case SDL_EVENT_WINDOW_HIDDEN:
        {
            pApp->pauseApp = true;
            break;
        }
        case SDL_EVENT_DID_ENTER_FOREGROUND:
        case SDL_EVENT_WINDOW_RESTORED:
        case SDL_EVENT_WINDOW_SHOWN:
        {
            pApp->pauseApp = false;
            break;
        }

        default:
            ;
    }

    // Forward events to specific modules that process SDL events
    for (auto& pModule : pApp->lowModules)
        pModule->ProcessEvent(pApp->ecs, event);
    for (auto& pModule : pApp->mediumModules)
        pModule->ProcessEvent(pApp->ecs, event);
    pApp->pAppLauncherModule->ProcessEvent(pApp->ecs, event);


    return pApp->quitApp ? SDL_APP_SUCCESS : SDL_APP_CONTINUE;
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

    if (!pApp->pauseApp)
        pApp->ecs.progress();
    
    return pApp->quitApp ? SDL_APP_SUCCESS : SDL_APP_CONTINUE;
}

void SDL_AppQuit(void* appstate) 
{
    AppState* pApp = (AppState*)appstate;

    if (pApp)
    {
        // Call OnExit on all modules (starting from high level modules)
        pApp->pAppLauncherModule->OnExit(pApp->ecs);

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
