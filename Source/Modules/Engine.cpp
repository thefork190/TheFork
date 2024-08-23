#include "Engine.h"

namespace Engine
{
    module::module(flecs::world& ecs) 
    {
        ecs.module<module>();

        ecs.component<Context>();
    }

    void KickstartEngine(flecs::world& ecs)
    {
        ecs.set<Context>({});
    }
}