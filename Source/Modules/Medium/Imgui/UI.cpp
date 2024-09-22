#include <set>
#include <map>
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

#define DEFAULT_IMGUI_FONT_ID UINT_MAX
#define DEFAULT_IMGUI_FONT_SIZE 13.f

namespace UI
{
    struct Context
    {
        bool isInitialized = false;
        float contentScale = 1.f;

        std::map<std::pair<unsigned int, unsigned int>, ImFont*> loadedFonts; // <fontId, size> -> ImFont*
        std::set<std::pair<unsigned int, unsigned int>> fontsToLoad; // <fontId, size>
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
            .each([](flecs::iter& it, size_t i, Engine::Canvas const& canvas, Window::SDLWindow const& sdlWin)
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
                    
                    // Cache content scale so we can handle it if it changes
                    SDL_DisplayID const dispId = SDL_GetDisplayForWindow(sdlWin.pWindow);
                    if (dispId == 0)
                    {
                        LOGF(eERROR, "SDL_GetDisplayForWindow() failed.");
                    }
                    else
                    {
                        pContext->contentScale = SDL_GetDisplayContentScale(dispId);
                    }

                    // Load default font
                    unsigned int actualFontSize = DEFAULT_IMGUI_FONT_SIZE * pContext->contentScale; // ImGui guidelines recommends getting the floor
                    ImFontConfig fontConfig = ImFontConfig();
                    fontConfig.SizePixels = static_cast<float>(actualFontSize);
                    ImFont* pDefaultFont = io.Fonts->AddFontDefault(&fontConfig);
                    ASSERT(pDefaultFont);

                    pContext->loadedFonts[{DEFAULT_IMGUI_FONT_ID, actualFontSize}] = pDefaultFont;

                    // Build the atlas texture
                    ImGui_TheForge_BuildFontAtlas(pRHI->pGfxQueue);

                    // Ensure style is setup for content scale
                    ImGui::GetStyle().ScaleAllSizes(pContext->contentScale);

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
            .each([](flecs::iter& it, size_t i, UI const& ui)
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
            .each([](flecs::iter& it, size_t i, Engine::Canvas const& canvas, Window::SDLWindow const& sdlWin)
                {
                    if (!it.world().has<Context>())
                        return;

                    Context* pContext = it.world().get_mut<Context>();
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

                    // Load new fonts if needed and rebuild the atlas
                    if (!pContext->fontsToLoad.empty())
                    {
                        // Functions not accessible via normal interface header
                        extern void* fntGetRawFontData(uint32_t fontID);
                        extern uint32_t fntGetRawFontDataSize(uint32_t fontID);

                        // Clear the imgui font atlas (this will invalidate all the ImFont pointers we cached)
                        ImGuiIO& io = ImGui::GetIO();
                        io.Fonts->Clear();

                        // Start by readding all the already loaded fonts
                        for (auto& loadedFontDesc : pContext->loadedFonts)
                        {
                            if (DEFAULT_IMGUI_FONT_ID == loadedFontDesc.first.first) // it was a default font
                            {
                                ImFontConfig fontConfig = ImFontConfig();
                                fontConfig.SizePixels = static_cast<float>(loadedFontDesc.first.second);
                                loadedFontDesc.second = io.Fonts->AddFontDefault(&fontConfig);
                                ASSERT(loadedFontDesc.second);
                            }
                            else
                            {
                                void* pFontBuffer = fntGetRawFontData(loadedFontDesc.first.first);
                                ASSERT(pFontBuffer);
                                uint32_t fontBufferSize = fntGetRawFontDataSize(loadedFontDesc.first.first);
                                
                                ImFontConfig config = {};
                                config.FontDataOwnedByAtlas = false;
                                loadedFontDesc.second = io.Fonts->AddFontFromMemoryTTF(pFontBuffer, fontBufferSize, loadedFontDesc.first.second, &config, nullptr);
                                ASSERT(loadedFontDesc.second);
                            }
                        }

                        // Now handle the new ones to load
                        for (auto const& newFontDesc : pContext->fontsToLoad)
                        {
                            void* pFontBuffer = fntGetRawFontData(newFontDesc.first);
                            ASSERT(pFontBuffer);
                            uint32_t fontBufferSize = fntGetRawFontDataSize(newFontDesc.first);

                            ImFontConfig config = {};
                            config.FontDataOwnedByAtlas = false;
                            ImFont* pFont = io.Fonts->AddFontFromMemoryTTF(pFontBuffer, fontBufferSize, newFontDesc.second, &config, nullptr);
                            ASSERT(pFont);
                            pContext->loadedFonts[{newFontDesc.first, newFontDesc.second}] = pFont;
                        }

                        // Rebuild the atlas
                        ImGui_TheForge_BuildFontAtlas(pRHI->pGfxQueue);

                        // All loaded, we can clear the fontsToLoad set
                        pContext->fontsToLoad.clear();
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
            // First check if a default font is there to fallback on for the current content scale
            ImFont* pDefaultFont = nullptr;

            std::pair<unsigned int, unsigned int> const defaultFontForCurContentScale = { DEFAULT_IMGUI_FONT_ID, static_cast<unsigned int>(DEFAULT_IMGUI_FONT_SIZE * pContext->contentScale) };
            std::pair<unsigned int, unsigned int> const defaultFont = { DEFAULT_IMGUI_FONT_ID, static_cast<unsigned int>(DEFAULT_IMGUI_FONT_SIZE) };

            if (pContext->loadedFonts.find(defaultFontForCurContentScale) != pContext->loadedFonts.end())
                pDefaultFont = pContext->loadedFonts.at(defaultFontForCurContentScale);
            else if (pContext->loadedFonts.find(defaultFont) != pContext->loadedFonts.end())
                pDefaultFont = pContext->loadedFonts.at(defaultFont);

            ASSERTMSG(pDefaultFont, "Default font was not loaded or cached!");
            if (!pDefaultFont)
                return nullptr;

            // Now see if the requested font is available
            unsigned int const fontId = FontRendering::InternalId(ecs, font);
            unsigned int const actualSize = size * pContext->contentScale;

            if (pContext->loadedFonts.find({ fontId, actualSize }) != pContext->loadedFonts.end())
            {
                // it is so return it
                return pContext->loadedFonts.at({ fontId, actualSize });
            }
            else
            {
                // need to load it.  Deffer it and return fallback
                pContext->fontsToLoad.insert({ fontId, actualSize });
                return pDefaultFont;
            }
        }
        
        return nullptr;
    }
}
