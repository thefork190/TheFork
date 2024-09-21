#include <set>
#include <utility> // For std::pair

#include <imgui.h>

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

        std::set<std::pair<unsigned int, float>> fontsToLoad;
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
                    ImGui_ImplTheForge_InitDesc initDesc = { pRHI->pRenderer, static_cast<unsigned int>(sdlWin.pSwapChain->ppRenderTargets[0]->mFormat) };
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
                    if (!it.world().has<Context>())
                        return;

                    Context const* pContext = it.world().get<Context>();
                    if (!pContext->isInitialized)
                        return;

                    ImGui_ImplSDL3_NewFrame();
                    ImGui_TheForge_NewFrame();
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

        ecs.system<Engine::Canvas, Window::SDLWindow>("UI Draw")
            .kind(Engine::GetCustomPhaseEntity(ecs, Engine::UI_RENDER))
            .each([](flecs::iter& it, size_t i, Engine::Canvas& canvas, Window::SDLWindow& sdlWin)
                {
                    if (!it.world().has<Context>())
                        return;

                    Context const* pContext = it.world().get<Context>();
                    if (!pContext->isInitialized)
                        return;

                    ASSERTMSG(i == 0, "Drawing to more than one window not implemented.");

                    // TODO: we always call end frame and that might not be very performant
                    //       The assumption is that the UI frame pacer system always calls ImGui::NewFrame, even if there's no UI
                    ImGui::EndFrame();

                    RHI::RHI const* pRHI = it.world().has<RHI::RHI>() ? it.world().get<RHI::RHI>() : nullptr;
                    
                    if (pRHI)
                    {
                        ImGui::Render();
                        ImDrawData* pDrawData = ImGui::GetDrawData();

                        if (pDrawData && pDrawData->Valid && pDrawData->TotalIdxCount > 0 && pDrawData->TotalVtxCount > 0)
                        {
                            Cmd* pCmd = pRHI->curCmdRingElem.pCmds[0];
                            ASSERT(pCmd);

                            cmdBeginDebugMarker(pCmd, 1, 0, 1, "ImGui Draw");

                            BindRenderTargetsDesc bindRenderTargets = {};
                            bindRenderTargets.mRenderTargetCount = 1;
                            bindRenderTargets.mRenderTargets[0] = { sdlWin.pCurRT, LOAD_ACTION_LOAD };
                            cmdBindRenderTargets(pCmd, &bindRenderTargets);

                            ImGui_TheForge_RenderDrawData(pDrawData, pCmd);

                            cmdBindRenderTargets(pCmd, nullptr);

                            cmdEndDebugMarker(pCmd);
                        }
                    }
                }
            );
    }

    void module::OnExit(flecs::world& ecs)
    {
        RHI::RHI const* pRHI = ecs.has<RHI::RHI>() ? ecs.get<RHI::RHI>() : nullptr;

        if (pRHI && pRHI->pRenderer)
        {
            ImGui_TheForge_Shutdown();
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

    bool WantsCaptureInputs(flecs::world& ecs)
    {
        Context const* pContext = ecs.has<Context>() ? ecs.get<Context>() : nullptr;

        if (pContext && pContext->isInitialized)
        {
            ImGuiIO& io = ImGui::GetIO();
            return io.WantCaptureMouse || io.WantCaptureKeyboard || io.NavVisible;
        }

        return false;
    }

    ImFont* GetOrAddFont(flecs::world& ecs, FontRendering::eAvailableFonts const font, float const size)
    {
        ASSERT(font < FontRendering::NUM_AVAILABLE_FONTS);

        Context* pContext = ecs.has<Context>() ? ecs.get_mut<Context>() : nullptr;

        if (pContext && pContext->isInitialized)
        {
            unsigned int const fontId = FontRendering::InternalId(ecs, font);
            if (ImGui_TheForge_FontIsLoaded(fontId, size))
                return ImGui_TheForge_GetOrAddFont(fontId, size);
            else
            {
                std::pair<unsigned int, float> const toAdd = { fontId, size };
                pContext->fontsToLoad.insert(toAdd);

                // Note: no need to modify changes, we'll get them after the following sync point.
                //       we don't really care since we're deferring font loading after ImGui::EndFrame()

                return ImGui_TheForge_GetFallbackFont();
            }
        }
        
        return nullptr;
    }
}
