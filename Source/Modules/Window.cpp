#include <SDL3/SDL.h>

#include "Engine.h"
#include "Window.h"

namespace Window
{
    module::module(flecs::world& ecs) 
    {
        ecs.import<Engine::module>();

        ecs.module<module>();

        ecs.component<SDLWindow>();

        auto createSDLWin = ecs.observer<Engine::Canvas>("SDL Window Creator")
            .each([](flecs::iter& it, size_t i, Engine::Canvas& canvas)
                {
                    //it[i].
                }
            );

    }
}