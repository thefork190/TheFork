#include <imgui.h>
//#include <backends/imgui_impl_tf.h>

#include <ILog.h>
#include <Graphics/GraphicsConfig.h>

#include "Low/Engine.h"
#include "Low/RHI.h"
#include "Low/Window.h"

#include "imgui_impl_sdl3.h"

#include "UI.h"

namespace UI
{
    struct Context
    {
        bool isInitialized = false;
        flecs::query<UIUpdater> uiUpdaterQuery;
    };

    module::module(flecs::world& ecs)
    {
        ecs.import<Engine::module>();
        ecs.import<RHI::module>();
        ecs.import<Window::module>();
        
        ecs.module<module>();

        ecs.component<Context>();
        ecs.component<UIUpdater>();

        // Create the context singleton
        Context context = {};
        context.uiUpdaterQuery = ecs.query_builder<UIUpdater>().cached().build();
        ecs.set<Context>(context);
        
        auto uiInitializer = ecs.system<Engine::Canvas, Window::SDLWindow>("Init UI")
            .kind(flecs::OnLoad)
            .each([](flecs::iter& it, size_t i, Engine::Canvas& canvas, Window::SDLWindow& sdlWin)
                {
                    ASSERTMSG(i == 0, "Only 1 window is supported.");

                    if (!it.world().has<Context>())
                        return;

                    Context* pContext = it.world().get_mut<Context>();
                    if (pContext->isInitialized)
                        return;

                    RHI::RHI const* pRHI = it.world().has<RHI::RHI>() ? it.world().get<RHI::RHI>() : nullptr;

                    if (!pRHI)
                        return;

                    if (!sdlWin.pSwapChain)
                        return;

                    IMGUI_CHECKVERSION();
                    ImGui::CreateContext();
                    ImGuiIO& io = ImGui::GetIO();
                    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
                    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
                    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // Enable docking

                    ImGui::StyleColorsDark();

                    // Load default font
                    io.Fonts->AddFontDefault();

                    ImGui_ImplSDL3_InitForOther(sdlWin.pWindow);

                    pContext->isInitialized = true;

                    // Ensure we notify modifications for following systems in the same phase that'll use the context
                    it.world().modified<Context>();
                }
            );
    }

    void module::OnExit(flecs::world& ecs)
    {
        RHI::RHI const* pRHI = ecs.has<RHI::RHI>() ? ecs.get<RHI::RHI>() : nullptr;

        if (pRHI && pRHI->pRenderer)
        {
            ImGui_ImplSDL3_Shutdown();
            ImGui::DestroyContext();
        }
        else
        {
            ASSERTMSG(0, "RHI is expected to be valid.");
        }
    }

    void ForwardEvent(flecs::world& ecs, const SDL_Event* sdlEvent)
    {
        Context const* pContext = ecs.has<Context>() ? ecs.get<Context>() : nullptr;

        if (pContext && pContext->isInitialized)
        {
            ImGui_ImplSDL3_ProcessEvent(sdlEvent);
        }
    }
}
