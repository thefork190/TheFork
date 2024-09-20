#include <imgui.h>
//#include <backends/imgui_impl_tf.h>

#include <ILog.h>
#include <Graphics/GraphicsConfig.h>

#include "Low/Engine.h"
#include "Low/RHI.h"
#include "Low/Window.h"

#include "imgui_impl_sdl3.h"
#include "imgui_impl_theforge.h"

#include "UI.h"

namespace UI
{
    struct Context
    {
        bool isInitialized = false;
    };

    module::module(flecs::world& ecs)
    {
        ecs.import<Engine::module>();
        ecs.import<RHI::module>();
        ecs.import<Window::module>();
        
        ecs.module<module>();

        ecs.component<Context>();
        ecs.component<UI>();

        // Create the context singleton
        Context context = {};
        ecs.set<Context>(context);
        
        ecs.system<Engine::Canvas, Window::SDLWindow>("UI Initializer")
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

                    ImGui_ImplSDL3_InitForOther(sdlWin.pWindow);
                    ImGui_ImplTheForge_InitDesc initDesc = { pRHI->pRenderer, static_cast<uint32_t>(sdlWin.pSwapChain->ppRenderTargets[0]->mFormat) };
                    ImGui_TheForge_Init(initDesc);

                    pContext->isInitialized = true;

                    // Ensure we notify modifications for following systems in the same phase that'll use the context
                    it.world().modified<Context>();
                }
            );

        ecs.system("UI Frame Pacer")
            .kind(flecs::OnLoad)
            .run([](flecs::iter& it)
                {
                    ImGui_ImplSDL3_NewFrame();
                    ImGui::NewFrame();
                }
            );

        ecs.system<UI>("UI Updater")
            .kind(flecs::PostUpdate) // Want to run UI updates after OnUpdate phase is done
            .each([](flecs::iter& it, size_t i, UI& ui)
                {
                    if (!it.world().has<Context>())
                        return;

                    Context const* pContext = it.world().get<Context>();
                    if (!pContext->isInitialized)
                        return;

                    auto world = it.world();
                    ui.Update(world);
                });
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
