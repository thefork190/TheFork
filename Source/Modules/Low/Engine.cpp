#include "Engine.h"
#include <ILog.h>

namespace Engine
{
    module::module(flecs::world& ecs) 
    {
        ecs.module<module>();

        ecs.component<Context>();
    }

    void KickstartEngine(flecs::world& ecs, std::string const* pAppName)
    {
        // Create the context
        ecs.set<Context>({});
        
        if (pAppName)
        {
            auto pContext = ecs.get_mut<Context>();
            ASSERT(pContext);
            pContext->SetAppName(*pAppName);
        }

        // Create a window entity with a canvas
        flecs::entity winEnt = ecs.entity("MainWindow");
        winEnt.set<Canvas>({ 1920, 1080 });
    }
}