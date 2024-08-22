#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <cmath>

// TF 
#include <ILog.h>
#include <IMemory.h>
#include <IFileSystem.h>


#define APP_NAME "TheFork"


struct AppContext {
    SDL_Window* window;
    bool app_quit = false;
};

int SDL_Fail(){
    LOGF(eERROR, "Error %s", SDL_GetError());
    return -1;
}

int SDL_AppInit(void** appstate, int argc, char* argv[]) {

    // Init TF stuff
    FileSystemInitDesc fsDesc = {};
    fsDesc.pAppName = APP_NAME;
    if (!initFileSystem(&fsDesc))
        return EXIT_FAILURE;

    fsSetPathForResourceDir(pSystemFileIO, RM_DEBUG, RD_LOG, "");

    initMemAlloc(APP_NAME);
    initLog(APP_NAME, DEFAULT_LOG_LEVEL);

    // init the library, here we make a window so we only need the Video capabilities.
    if (SDL_Init(SDL_INIT_VIDEO)){
        return SDL_Fail();
    }
    
    // create a window
    SDL_Window* window = SDL_CreateWindow("Window", 352, 430, SDL_WINDOW_RESIZABLE);
    if (!window){
        return SDL_Fail();
    }

    // print some information about the window
    SDL_ShowWindow(window);
    {
        int width, height, bbwidth, bbheight;
        SDL_GetWindowSize(window, &width, &height);
        SDL_GetWindowSizeInPixels(window, &bbwidth, &bbheight);
        LOGF(eINFO, "Window size: %ix%i", width, height);
        LOGF(eINFO, "Backbuffer size: %ix%i", bbwidth, bbheight);
        if (width != bbwidth){
            LOGF(eINFO, "This is a highdpi environment.");
        }
    }

    // set up the application data
    *appstate = tf_new(AppContext);
    ((AppContext*)(*appstate))->window = window;
    
    LOGF(eINFO, "Application started successfully!");

    return 0;
}

int SDL_AppEvent(void *appstate, const SDL_Event* event) {
    auto* app = (AppContext*)appstate;
    
    if (event->type == SDL_EVENT_QUIT) {
        app->app_quit = SDL_TRUE;
    }

    return 0;
}

int SDL_AppIterate(void *appstate) {
    auto* app = (AppContext*)appstate;
    
    return app->app_quit;
}

void SDL_AppQuit(void* appstate) {
    auto* app = (AppContext*)appstate;
    if (app) {
        SDL_DestroyWindow(app->window);
        tf_delete(app);
    }

    SDL_Quit();
    LOGF(eINFO, "Application quit successfully!");

    exitFileSystem();
    exitLog();
    exitMemAlloc();

}
