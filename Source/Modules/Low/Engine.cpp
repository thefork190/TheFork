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
        // Create the context
        ecs.set<Context>({});

        // Create a window entity with a canvas
        flecs::entity winEnt = ecs.entity("MainWindow");
        winEnt.set<Canvas>({ 1920, 1080 });
    }
}