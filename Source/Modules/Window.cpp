#include <ILog.h>

#include "Engine.h"
#include "RHI.h"
#include "Window.h"

namespace Window
{
    module::module(flecs::world& ecs)
    {
        ecs.import<Engine::module>();
        ecs.import<RHI::module>();

        ecs.module<module>();

        ecs.component<SDLWindow>()
            .on_add([](flecs::entity e, SDLWindow& sdlWin)
                {
                    unsigned int w = 512;
                    unsigned int h = 512;

                    if (e.has<Engine::Canvas>())
                    {
                        Engine::Canvas const* const canvas = e.get<Engine::Canvas>();
                        w = canvas->width;
                        h = canvas->height;
                    }

                    sdlWin.pWindow = SDL_CreateWindow(APP_NAME, 1920, 1080, SDL_WINDOW_RESIZABLE | SDL_WINDOW_VULKAN);
                    if (!sdlWin.pWindow)
                    {
                        ASSERTMSG(eERROR, "SDL failed to create window.");
                        return;
                    }

                    SDL_ShowWindow(sdlWin.pWindow);

                    int width, height, bbwidth, bbheight;
                    SDL_GetWindowSize(sdlWin.pWindow, &width, &height);
                    SDL_GetWindowSizeInPixels(sdlWin.pWindow, &bbwidth, &bbheight);
                    LOGF(eINFO, "SDL window created for Canvas");
                    LOGF(eINFO, "Window size: %ix%i", width, height);
                    LOGF(eINFO, "Backbuffer size: %ix%i", bbwidth, bbheight);
                    if (width != bbwidth)
                    {
                        LOGF(eINFO, "High dpi detected.");
                    }

                    auto pRHI = e.world().get_mut<RHI::RHI>();
                    ASSERT(pRHI);

                    // TODO this is platform specific
                    HWND pWinHandle = (HWND)SDL_GetPointerProperty(SDL_GetWindowProperties(sdlWin.pWindow), SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);
                    ASSERT(pWinHandle);

                    SwapChainDesc swapChainDesc = {};

                    WindowHandle tfWindowHandle = {};
                    tfWindowHandle.window = pWinHandle;
                    tfWindowHandle.type = WINDOW_HANDLE_TYPE_WIN32;
                    //

                    swapChainDesc.mWindowHandle = tfWindowHandle;
                    swapChainDesc.mPresentQueueCount = 1;
                    swapChainDesc.ppPresentQueues = &pRHI->pGfxQueue;
                    swapChainDesc.mWidth = bbwidth;
                    swapChainDesc.mHeight = bbheight;
                    swapChainDesc.mImageCount = getRecommendedSwapchainImageCount(pRHI->pRenderer, &tfWindowHandle);
                    swapChainDesc.mColorFormat = getSupportedSwapchainFormat(pRHI->pRenderer, &swapChainDesc, COLOR_SPACE_SDR_SRGB);
                    swapChainDesc.mColorSpace = COLOR_SPACE_SDR_SRGB;
                    swapChainDesc.mEnableVsync = true; // TODO: allow disabling?
                    swapChainDesc.mFlags = SWAP_CHAIN_CREATION_FLAG_ENABLE_FOVEATED_RENDERING_VR;
                    addSwapChain(pRHI->pRenderer, &swapChainDesc, &sdlWin.pSwapChain);
                    ASSERT(sdlWin.pSwapChain);

                    addSemaphore(pRHI->pRenderer, &sdlWin.pImgAcqSemaphore);
                }
            )
            .on_remove([](flecs::entity e, SDLWindow& sdlWin)
                {
                    auto pRHI = e.world().get_mut<RHI::RHI>();
                    ASSERT(pRHI);

                    removeSemaphore(pRHI->pRenderer, sdlWin.pImgAcqSemaphore);
                    sdlWin.pImgAcqSemaphore = nullptr;

                    removeSwapChain(pRHI->pRenderer, sdlWin.pSwapChain);
                    sdlWin.pSwapChain = nullptr;

                    SDL_DestroyWindow(sdlWin.pWindow);
                    sdlWin.pWindow = nullptr;
                }
            );
        
        auto createSDLWin = ecs.observer<Engine::Canvas>("SDL Window Creator")
            .event(flecs::OnSet)
            .each([](flecs::iter& it, size_t i, Engine::Canvas& canvas)
                {
                    it.entity(i).add<SDLWindow>();
                }
            );
       
        auto swapchainResizer = ecs.system<Engine::Canvas, Window::SDLWindow>("Swapchain Resizer")
            .kind(flecs::OnLoad)
            .each([](flecs::iter& it, size_t i, Engine::Canvas& canvas, Window::SDLWindow& sdlWin)
                {
                    int bbwidth, bbheight;
                    SDL_GetWindowSizeInPixels(sdlWin.pWindow, &bbwidth, &bbheight);

                    if ((canvas.width != bbwidth) ||
                        (canvas.height != bbheight))
                    {
                        LOGF(eDEBUG, "Window was resized to %ix%i", bbwidth, bbheight);
                        LOGF(eDEBUG, "[IMPLEMENT WINDOW RESIZING]");
                    }
                }
            );

        auto acquireNextImg = ecs.system<Window::SDLWindow>("Acquire Next Img")
            .kind(flecs::PreUpdate)
            .each([](flecs::iter& it, size_t i, Window::SDLWindow& sdlWin)
                {
                    ASSERTMSG(i == 0, "More than one window not implemented.");

                    auto pRHI = it.world().get_mut<RHI::RHI>();
                    if (pRHI)
                    {
                        uint32_t swapchainImageIndex;
                        acquireNextImage(pRHI->pRenderer, sdlWin.pSwapChain, sdlWin.pImgAcqSemaphore, nullptr, &swapchainImageIndex);

                        sdlWin.pCurRT = sdlWin.pSwapChain->ppRenderTargets[swapchainImageIndex];

                        // Stall if CPU is running "gDataBufferCount" frames ahead of GPU
                        sdlWin.curCmdRingElem = getNextGpuCmdRingElement(&pRHI->gfxCmdRing, true, 1);
                        FenceStatus fenceStatus;
                        getFenceStatus(pRHI->pRenderer, sdlWin.curCmdRingElem.pFence, &fenceStatus);
                        if (fenceStatus == FENCE_STATUS_INCOMPLETE)
                            waitForFences(pRHI->pRenderer, 1, &sdlWin.curCmdRingElem.pFence);

                        // Reset cmd pool for this frame
                        resetCmdPool(pRHI->pRenderer, sdlWin.curCmdRingElem.pCmdPool);
                    }
                }
            );
    }
}