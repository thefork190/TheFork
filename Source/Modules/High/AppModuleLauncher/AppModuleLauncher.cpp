#include <vector>
#include <functional>

#include "LifeCycledModule.h"

#include "Low/Engine.h"
#include "Low/Inputs.h"
#include "Low/RHI.h"
#include "Low/Window.h"
#include "Medium/Imgui/UI.h"

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
        ecs.import<RHI::module>();
        ecs.import<Window::module>();
        ecs.import<Engine::module>();

        ecs.module<module>();

        for (auto const& appModule : gAvailableAppModules)
        {
            if (AppModuleToStart() == appModule.name)
            {
                appModule.Start(ecs);
                break;
            }
        }

        // If mode wasn't found or one wasn't specified, bring up UI launcher.
        UI::UI ui = {};
        ui.Update = [](flecs::world& ecs)
            {
                // Uncomment for Imgui demo (has useful tools when building UI)
                //ImGui::ShowDemoWindow(nullptr);

                std::vector<const char*> appNames;
                appNames.reserve(gAvailableAppModules.size());
                std::vector<const char*> appInfos;
                appInfos.reserve(gAvailableAppModules.size());

                for (auto const& appModule : gAvailableAppModules)
                {
                    appNames.push_back(appModule.name.c_str());
                    appInfos.push_back(appModule.info.c_str());
                }

                bool open = true;
                ImGui::SetNextWindowSize(ImVec2(512, 256));
                ImGui::Begin("App Launcher", &open, ImGuiWindowFlags_NoDecoration);

                static int selectedAppIndex = 0;
                if (ImGui::BeginCombo("App Selection", appNames[selectedAppIndex], 0))
                {
                    for (int n = 0; n < gAvailableAppModules.size(); n++)
                    {
                        bool const isSelected = (selectedAppIndex == n);
                        if (ImGui::Selectable(appNames[n], isSelected))
                            selectedAppIndex = n;

                        // Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
                        if (selectedAppIndex)
                            ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }

                ImGui::TextWrapped(appInfos[selectedAppIndex]);

                if (ImGui::Button("LAUNCH"))
                {

                }

                ImGui::End();
            };
        ecs.entity("AppModuleLauncher::UI").set<UI::UI>(ui);

        // Clear screen
        ecs.system<Engine::Canvas, Window::SDLWindow>("AppModuleLauncher::Draw")
            .kind(flecs::OnStore)
            .each([](flecs::iter& it, size_t i, Engine::Canvas const& canvas, Window::SDLWindow const& sdlWin)
                {
                    ASSERTMSG(i == 0, "Drawing to more than one window not implemented.");

                    RHI::RHI const* pRHI = it.world().has<RHI::RHI>() ? it.world().get<RHI::RHI>() : nullptr;
                    
                    if (pRHI && sdlWin.pCurRT)
                    {
                        Cmd* pCmd = pRHI->curCmdRingElem.pCmds[0];
                        ASSERT(pCmd);

                        BindRenderTargetsDesc bindRenderTargets = {};
                        bindRenderTargets.mRenderTargetCount = 1;
                        bindRenderTargets.mRenderTargets[0] = { sdlWin.pCurRT, LOAD_ACTION_CLEAR };
                        cmdBindRenderTargets(pCmd, &bindRenderTargets);
                        cmdSetViewport(pCmd, 0.0f, 0.0f, (float)canvas.width, (float)canvas.height, 0.0f, 1.0f);
                        cmdSetScissor(pCmd, 0, 0, canvas.width, canvas.height);

                        cmdBindRenderTargets(pCmd, nullptr);
                    }
                }
            );
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