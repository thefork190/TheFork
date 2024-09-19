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
        flecs::query<UI> uiUpdaterQuery;
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
        context.uiUpdaterQuery = ecs.query_builder<UI>().cached().build();
        ecs.set<Context>(context);
        
        auto uiInitializer = ecs.system<Engine::Canvas, Window::SDLWindow>("UI Initializer")
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

        //auto uiUpdater = ecs.system<UI>("UI Updater")
        //    .kind(flecs::OnUpdate)
        //    .each([](flecs::iter& it, size_t i, Engine::Canvas& canvas, Window::SDLWindow& sdlWin)
        //        {
        //            ASSERTMSG(i == 0, "Drawing to more than one window not implemented.");

        //            if (!it.world().has<Context>())
        //                return;

        //            Context const* pContext = it.world().get<Context>();
        //            if (!pContext->isInitialized)
        //                return;

        //            RHI::RHI const* pRHI = it.world().has<RHI::RHI>() ? it.world().get<RHI::RHI>() : nullptr;

        //            if (!pRHI)
        //                return;

        //            if (!sdlWin.pCurRT)
        //                return;

        //            Cmd* pCmd = pRHI->curCmdRingElem.pCmds[0];
        //            ASSERT(pCmd);


        //            unsigned int runIterations = 0;

        //            pContext->fontTextQuery.run([pContext, pRHI, pCmd, &runIterations, sdlWin, canvas](flecs::iter& it)
        //                {
        //                    while (it.next())
        //                    {

        //                        auto fontTexts = it.field<FontText const>(0);

        //                        for (size_t j : it)
        //                        {
        //                            if (runIterations == 0) // on first iteration, setup the render pass
        //                            {
        //                                cmdBeginDebugMarker(pCmd, 1, 0, 1, "FontRendering::Render");

        //                                BindRenderTargetsDesc bindRenderTargets = {};
        //                                bindRenderTargets.mRenderTargetCount = 1;
        //                                bindRenderTargets.mRenderTargets[0] = { sdlWin.pCurRT, LOAD_ACTION_LOAD };
        //                                cmdBindRenderTargets(pCmd, &bindRenderTargets);
        //                                cmdSetViewport(pCmd, 0.0f, 0.0f, (float)canvas.width, (float)canvas.height, 0.0f, 1.0f);
        //                                cmdSetScissor(pCmd, 0, 0, canvas.width, canvas.height);
        //                            }

        //                            // Validate incoming data
        //                            if (pContext->fontNameToIdMap.find(fontTexts[j].font) == pContext->fontNameToIdMap.end())
        //                            {
        //                                LOGF(eWARNING, "Could not find font ID for entity FontText component.");
        //                                return;
        //                            }

        //                            FontDrawDesc desc = {};
        //                            desc.mFontBlur = fontTexts[j].fontBlur;
        //                            desc.mFontColor = fontTexts[j].color;
        //                            desc.mFontID = pContext->fontNameToIdMap.at(fontTexts[j].font);
        //                            desc.mFontSize = fontTexts[j].fontSize;
        //                            desc.mFontSpacing = fontTexts[j].fontSpacing;
        //                            desc.pText = fontTexts[j].text.c_str();

        //                            cmdDrawTextWithFont(pCmd, { fontTexts[j].posX, fontTexts[j].posY }, &desc);

        //                            runIterations += 1;
        //                        }
        //                    }
        //                });

        //            if (runIterations > 0) // need to clean things up if anything was ran
        //            {
        //                cmdBindRenderTargets(pCmd, nullptr);
        //                cmdEndDebugMarker(pCmd);
        //            }
        //        });
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
