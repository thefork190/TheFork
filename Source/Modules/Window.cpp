#include <ILog.h>

#include "Engine.h"
#include "Window.h"

namespace Window
{
    module::module(flecs::world& ecs) 
    {
        ecs.import<Engine::module>();

        ecs.module<module>();

        ecs.component<SDLWindow>()
            .on_add([](flecs::entity e, SDLWindow& sdlWin)
                {
                    unsigned int w = 512;
                    unsigned int h = 512;
                    
                    if (e.has<Engine::Canvas>())
                    {
                        Engine::Canvas const* const canvas = e.get<Engine::Canvas>();
                        w = canvas->width;
                        h = canvas->height;
                    }

                    sdlWin.pWindow = SDL_CreateWindow(APP_NAME, 1920, 1080, SDL_WINDOW_RESIZABLE | SDL_WINDOW_VULKAN);
                    if (!sdlWin.pWindow)
                    {
                        ASSERTMSG(eERROR, "SDL failed to create window.");
                        return;
                    }

                    SDL_ShowWindow(sdlWin.pWindow);
                    {
                        int width, height, bbwidth, bbheight;
                        SDL_GetWindowSize(sdlWin.pWindow, &width, &height);
                        SDL_GetWindowSizeInPixels(sdlWin.pWindow, &bbwidth, &bbheight);
                        LOGF(eINFO, "SDL window created for Canvas");
                        LOGF(eINFO, "Window size: %ix%i", width, height);
                        LOGF(eINFO, "Backbuffer size: %ix%i", bbwidth, bbheight);
                        if (width != bbwidth)
                        {
                            LOGF(eINFO, "High dpi detected.");
                        }
                    }
                }
            )
            .on_remove([](flecs::entity e, SDLWindow& sdlWin)
                {
                    SDL_DestroyWindow(sdlWin.pWindow);
                    sdlWin.pWindow = nullptr;
                }
            );

        auto createSDLWin = ecs.observer<Engine::Canvas>("SDL Window Creator")
            .event(flecs::OnSet)
            .each([](flecs::iter& it, size_t i, Engine::Canvas& canvas)
                {
                    it.entity(i).add<SDLWindow>();
                }
            );

        //auto swapchainCreator = ecs.system<Engine::Canvas, Window::SDLWindow>("Swapchain Creator")
        //    .kind(flecs::OnStart)

        auto swapchainResizer = ecs.system<Engine::Canvas, Window::SDLWindow>("Swapchain Resizer")
            .kind(flecs::PreFrame)
            .each([](flecs::iter& it, size_t i, Engine::Canvas& canvas, Window::SDLWindow& sdlWindow)
                {
                    int bbwidth, bbheight;
                    SDL_GetWindowSizeInPixels(sdlWindow.pWindow, &bbwidth, &bbheight);

                    if ((canvas.width != bbwidth) ||
                        (canvas.height != bbheight))
                    {
                        LOGF(eDEBUG, "Window was resized to %ix%i", bbwidth, bbheight);
                    }
                }
            );

    }
}