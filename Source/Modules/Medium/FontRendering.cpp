#include <unordered_map>

#include <ILog.h>
#include <IFont.h>

#include <SDL3/SDL_video.h>

#include "Low/Engine.h"
#include "Low/RHI.h"
#include "Low/Window.h"
#include "FontRendering.h"

namespace FontRendering
{
    // The font rendering context (singleton)
    struct Context
    {
        bool isInitialized = false;
        unsigned int width = 0;
        unsigned int height = 0;
        std::unordered_map<eAvailableFonts, unsigned int> fontNameToIdMap;
        flecs::query<FontText const> fontTextQuery;
    };

    module::module(flecs::world& ecs)
    {
        ecs.import<Engine::module>();
        ecs.import<RHI::module>();
        ecs.import<Window::module>();
        
        ecs.module<module>();

        ecs.component<Context>();
        ecs.component<FontText>();

        // Create the context singleton
        Context context = {};
        context.fontTextQuery = ecs.query_builder<FontText const>().cached().build();
        ecs.set<Context>(context);
        
        auto fontSysInitializer = ecs.system<Engine::Canvas, Window::SDLWindow>("Init Font System")
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

                    SDL_DisplayID const dispId = SDL_GetDisplayForWindow(sdlWin.pWindow);
                    if (dispId == 0)
                    {
                        LOGF(eERROR, "SDL_GetDisplayForWindow() failed.");
                    }

                    float const contentScale = SDL_GetDisplayContentScale(dispId);

                    FontSystemDesc fontSystemDesc = {};
                    fontSystemDesc.mColorFormat = sdlWin.pSwapChain->ppRenderTargets[0]->mFormat;
                    fontSystemDesc.mWidth = canvas.width;
                    fontSystemDesc.mHeight = canvas.height;
                    fontSystemDesc.mDpiDesc[0] = contentScale;
                    fontSystemDesc.mDpiDesc[1] = contentScale;
                    fontSystemDesc.pRenderer = pRHI->pRenderer;
                    if (!initFontSystem(&fontSystemDesc))
                    {
                        ASSERTMSG(eERROR, "Failed to init TF font system.");
                        return;
                    }

                    resizeFontSystem(canvas.width, canvas.height);

                    // Define available font names to asset files and create context singleton
                    std::unordered_map<eAvailableFonts, std::string> availableFontToAssetName;
                    availableFontToAssetName[COMIC_RELIEF] = "ComicRelief.ttf";
                    availableFontToAssetName[COMIC_RELIEF_BOLD] = "ComicRelief-Bold.ttf";
                    availableFontToAssetName[CRIMSON_BOLD] = "Crimson-Bold.ttf";
                    availableFontToAssetName[CRIMSON_BOLD_ITALIC] = "Crimson-BoldItalic.ttf";
                    availableFontToAssetName[CRIMSON_ITALIC] = "Crimson-Italic.ttf";
                    availableFontToAssetName[CRIMSON_ROMAN] = "Crimson-Roman.ttf";
                    availableFontToAssetName[CROMSON_SEMI_BOLD] = "Crimson-Semibold.ttf";
                    availableFontToAssetName[CRIMSON_SEMI_BOLD_ITALIC] = "Crimson-SemiboldItalic.ttf";
                    availableFontToAssetName[HERMENEUS_ONE] = "HermeneusOne.ttf";
                    availableFontToAssetName[INCONSOLATA_LGC] = "Inconsolata-LGC.otf";
                    availableFontToAssetName[INCONSOLATA_LGC_BOLD] = "Inconsolata-LGC-Bold.otf";
                    availableFontToAssetName[INCONSOLATA_LGC_BOLD_ITALIC] = "Inconsolata-LGC-BoldItalic.otf";
                    availableFontToAssetName[INCONSOLATA_LGC_ITALIC] = "Inconsolata-LGC-Italic.otf";
                    availableFontToAssetName[TITILLIUM_TEXT_BOLD] = "TitilliumText-Bold.otf";

                    unsigned int fontIds[NUM_AVAILABLE_FONTS] = {};

                    FontDesc fontDescs[NUM_AVAILABLE_FONTS] = {};
                    for (unsigned int i = 0; i < NUM_AVAILABLE_FONTS; ++i)
                    {
                        // Use the asset path for the name (not sure what the name is used for exactly)
                        fontDescs[i].pFontName = availableFontToAssetName[static_cast<eAvailableFonts>(i)].c_str();
                        fontDescs[i].pFontPath = availableFontToAssetName[static_cast<eAvailableFonts>(i)].c_str();
                    }

                    fntDefineFonts(fontDescs, NUM_AVAILABLE_FONTS, &fontIds[0]);

                    for (unsigned int i = 0; i < NUM_AVAILABLE_FONTS; ++i)
                    {
                        pContext->fontNameToIdMap[static_cast<eAvailableFonts>(i)] = fontIds[i];
                    }

                    pContext->width = canvas.width;
                    pContext->height = canvas.height;
                    pContext->isInitialized = true;

                    // Ensure we notify modifications for following systems in the same phase that'll use the context
                    it.world().modified<Context>();
                }
            );
       
        auto fontSysResizer = ecs.system<Engine::Canvas>("Font System Resizer")
            .kind(flecs::OnLoad)
            .each([](flecs::iter& it, size_t i, Engine::Canvas& canvas)
                {
                    ASSERTMSG(i == 0, "Only 1 window is supported.");

                    if (!it.world().has<Context>())
                        return;

                    Context* pContext = it.world().get_mut<Context>();
                    if (!pContext->isInitialized)
                        return;

                    if (!(pContext->width == canvas.width && pContext->height == canvas.height))
                    {
                        resizeFontSystem(canvas.width, canvas.height);
                        pContext->width = canvas.width;
                        pContext->height = canvas.height;
                    }
                }
            );

        auto fontRenderer = ecs.system<Engine::Canvas, Window::SDLWindow>("Font Renderer")
            .kind(Engine::GetCustomPhaseEntity(ecs, Engine::FONTS_RENDER))
            .each([](flecs::iter& it, size_t i, Engine::Canvas& canvas, Window::SDLWindow& sdlWin)
                {
                    ASSERTMSG(i == 0, "Drawing to more than one window not implemented.");

                    if (!it.world().has<Context>())
                        return;

                    Context const* pContext = it.world().get<Context>();
                    if (!pContext->isInitialized)
                        return;

                    RHI::RHI const* pRHI = it.world().has<RHI::RHI>() ? it.world().get<RHI::RHI>() : nullptr;

                    if (!pRHI)
                        return;

                    if (!sdlWin.pCurRT)
                        return;

                    Cmd* pCmd = pRHI->curCmdRingElem.pCmds[0];
                    ASSERT(pCmd);


                    unsigned int runIterations = 0;

                    pContext->fontTextQuery.run([pContext, pRHI, pCmd, &runIterations, sdlWin, canvas](flecs::iter& it)
                        {
                            while (it.next())
                            {

                                auto fontTexts = it.field<FontText const>(0);

                                for (size_t j : it)
                                {
                                    if (runIterations == 0) // on first iteration, setup the render pass
                                    {
                                        cmdBeginDebugMarker(pCmd, 1, 0, 1, "FontRendering::Render");

                                        BindRenderTargetsDesc bindRenderTargets = {};
                                        bindRenderTargets.mRenderTargetCount = 1;
                                        bindRenderTargets.mRenderTargets[0] = { sdlWin.pCurRT, LOAD_ACTION_LOAD };
                                        cmdBindRenderTargets(pCmd, &bindRenderTargets);
                                        cmdSetViewport(pCmd, 0.0f, 0.0f, (float)canvas.width, (float)canvas.height, 0.0f, 1.0f);
                                        cmdSetScissor(pCmd, 0, 0, canvas.width, canvas.height);
                                    }

                                    // Validate incoming data
                                    if (pContext->fontNameToIdMap.find(fontTexts[j].font) == pContext->fontNameToIdMap.end())
                                    {
                                        LOGF(eWARNING, "Could not find font ID for entity FontText component.");
                                        return;
                                    }

                                    FontDrawDesc desc = {};
                                    desc.mFontBlur = fontTexts[j].fontBlur;
                                    desc.mFontColor = fontTexts[j].color;
                                    desc.mFontID = pContext->fontNameToIdMap.at(fontTexts[j].font);
                                    desc.mFontSize = fontTexts[j].fontSize;
                                    desc.mFontSpacing = fontTexts[j].fontSpacing;
                                    desc.pText = fontTexts[j].text.c_str();

                                    cmdDrawTextWithFont(pCmd, { fontTexts[j].posX, fontTexts[j].posY }, &desc);

                                    runIterations += 1;
                                }
                            }
                        });

                    if (runIterations > 0) // need to clean things up if anything was ran
                    {
                        cmdBindRenderTargets(pCmd, nullptr);
                        cmdEndDebugMarker(pCmd);
                    }
                });
    }

    void module::OnExit(flecs::world& ecs)
    {
        RHI::RHI const* pRHI = ecs.has<RHI::RHI>() ? ecs.get<RHI::RHI>() : nullptr;

        if (pRHI && pRHI->pRenderer)
        {
            exitFontSystem();
        }
        else
        {
            ASSERTMSG(0, "RHI is expected to be valid.");
        }
    }

    void MeasureText(flecs::world const& ecs, FontText const& fontText, float& xOut, float& yOut)
    {
        xOut = 0.f;
        yOut = 0.f;

        if (!ecs.has<Context>())
            return;

        Context const* pContext = ecs.get<Context>();
        if (!pContext->isInitialized)
            return;

        FontDrawDesc desc = {};
        desc.mFontBlur = fontText.fontBlur;
        desc.mFontColor = fontText.color;
        desc.mFontID = pContext->fontNameToIdMap.at(fontText.font);
        desc.mFontSize = fontText.fontSize;
        desc.mFontSpacing = fontText.fontSpacing;
        desc.pText = fontText.text.c_str();

        auto sizes = fntMeasureFontText(fontText.text.c_str(), &desc);

        xOut = sizes.x;
        yOut = sizes.y;
    }

    uint32_t InternalId(flecs::world& ecs, eAvailableFonts const font)
    {
        if (!ecs.has<Context>())
            return 0;

        Context const* pContext = ecs.get<Context>();
        if (!pContext->isInitialized)
            return 0;

        // All available fonts should be loaded at init time
        ASSERT(pContext->fontNameToIdMap.find(font) != pContext->fontNameToIdMap.end());

        return pContext->fontNameToIdMap.at(font);
    }
}
