#include <vector>
#include <functional>
#include "LifeCycledModule.h"
#include "AppModuleLauncher.h"

// To make an app "high" module available for usage, add include here
#include "High/FlappyClone/FlappyClone.h"
#include "High/HelloTriangle/HelloTriangle.h"

namespace AppModuleLauncher
{
    struct AppModule
    {
        std::string name;
        std::string info;
        std::function<void(flecs::world&)> Start;
    };

    LifeCycledModule* pLaunchedAppModule = nullptr;

    // Fill out apps here
    std::vector<AppModule> const gAvailableAppModules =
    {
        {
            "Hello Triangle",
            "Barebone application module that draws a triangle and provides a simple ImGui UI.",
            [](flecs::world & ecs) { pLaunchedAppModule = ecs.import<HelloTriangle::module>().get_mut<HelloTriangle::module>(); }
        },

        {
            "Flappy Clone",
            "A Flappy Bird clone that is fully implemented with ECS.",
            [](flecs::world & ecs) { pLaunchedAppModule = ecs.import<FlappyClone::module>().get_mut<FlappyClone::module>(); }
        },
    };

    module::module(flecs::world& ecs)
    {
        ecs.module<module>();

        for (auto const& appModule : gAvailableAppModules)
        {
            if (AppModuleToStart() == appModule.name)
            {
                appModule.Start(ecs);
                break;
            }
        }
    }

    void module::OnExit(flecs::world& ecs)
    {
        if (pLaunchedAppModule)
            pLaunchedAppModule->OnExit(ecs);

        pLaunchedAppModule = nullptr;
    }

    void module::ProcessEvent(flecs::world& ecs, const SDL_Event* sdlEvent)
    {
        if (pLaunchedAppModule)
            pLaunchedAppModule->ProcessEvent(ecs, sdlEvent);
    }
}