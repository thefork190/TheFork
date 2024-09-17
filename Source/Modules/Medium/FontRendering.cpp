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
    };

    module::module(flecs::world& ecs)
    {
        ecs.import<Engine::module>();
        ecs.import<Window::module>();
        
        ecs.module<module>();

        // Create the context singleton
        ecs.add<Context>();
        
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

                    Context const* pContext = it.world().get<Context>();
                    if (!pContext->isInitialized)
                        return;

                    if (!(pContext->width == canvas.width && pContext->height == canvas.height))
                    {
                        ASSERT(0 && "Implement me!");
                    }
                }
            );
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
}
