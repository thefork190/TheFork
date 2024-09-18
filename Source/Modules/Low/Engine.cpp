#include "Engine.h"
#include <ILog.h>

namespace Engine
{
    module::module(flecs::world& ecs) 
    {
        ecs.module<module>();

        ecs.component<Context>();

        // Create custom FLECS phases
        flecs::entity FontsRenderPhase = ecs.entity("FontsRenderPhase")
            .add(flecs::Phase)
            .depends_on(flecs::OnStore);

        flecs::entity UIRenderPhase = ecs.entity("UIRenderPhase")
            .add(flecs::Phase)
            .depends_on(FontsRenderPhase);

        flecs::entity PresentationPhase = ecs.entity("PresentationPhase")
            .add(flecs::Phase)
            .depends_on(UIRenderPhase);
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

    flecs::entity GetCustomPhaseEntity(flecs::world& ecs, eCustomPhase const& phase)
    {
        flecs::entity ret = {};

        switch (phase)
        {
        case Engine::FONTS_RENDER:
            ret = ecs.lookup("Engine::module::FontsRenderPhase");
            break;
        case Engine::UI_RENDER:
            ret = ecs.lookup("Engine::module::UIRenderPhase");
            break;
        case Engine::PRESENT:
            ret = ecs.lookup("Engine::module::PresentationPhase");
            break;
        default:
            ASSERTMSG(0, "Unknown custom phase.");
        }

        ASSERTMSG(ret.is_valid(), "Custom phase entity isn't valid.");

        return ret;
    }
}