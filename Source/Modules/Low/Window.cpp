#include <ILog.h>

#include "Engine.h"
#include "RHI.h"
#include "Window.h"

#define DEBUG_PRESENTATION_CLEAR_COLOR_RED 0
#if defined(__APPLE__)
#define HWND NSWindow *
#define WINDOW_PROP SDL_PROP_WINDOW_COCOA_WINDOW_POINTER
#define WINDOW_FLAGS SDL_WINDOW_RESIZABLE | SDL_WINDOW_METAL | SDL_WINDOW_HIGH_PIXEL_DENSITY
#else
#define WINDOW_PROP SDL_PROP_WINDOW_WIN32_HWND_POINTER
#define __bridge
#define WINDOW_FLAGS SDL_WINDOW_RESIZABLE | SDL_WINDOW_VULKAN
#endif

namespace Window
{
    void CreateWindowSwapchain(RHI::RHI* pRHI, SDLWindow& sdlWin, int const w, int const h)
    {
        // TODO this is platform specific
        HWND pWinHandle = (__bridge HWND)SDL_GetPointerProperty(SDL_GetWindowProperties(sdlWin.pWindow), WINDOW_PROP, NULL);
        ASSERT(pWinHandle);

        SwapChainDesc swapChainDesc = {};

        WindowHandle tfWindowHandle = {};
#if defined(__APPLE__)
        tfWindowHandle.window = sdlWin.pView;
#else
        tfWindowHandle.window = pWinHandle;
#endif
        tfWindowHandle.type = WINDOW_HANDLE_TYPE_WIN32;

        swapChainDesc.mWindowHandle = tfWindowHandle;
        swapChainDesc.mPresentQueueCount = 1;
        swapChainDesc.ppPresentQueues = &pRHI->pGfxQueue;
        swapChainDesc.mWidth = w;
        swapChainDesc.mHeight = h;
        swapChainDesc.mImageCount = getRecommendedSwapchainImageCount(pRHI->pRenderer, &tfWindowHandle);
        swapChainDesc.mColorFormat = getSupportedSwapchainFormat(pRHI->pRenderer, &swapChainDesc, COLOR_SPACE_SDR_SRGB);
        swapChainDesc.mColorSpace = COLOR_SPACE_SDR_SRGB;
#if DEBUG_PRESENTATION_CLEAR_COLOR_RED
        swapChainDesc.mColorClearValue.r = 1.f;
#endif
        swapChainDesc.mEnableVsync = true; // TODO: allow disabling?
        swapChainDesc.mFlags = SWAP_CHAIN_CREATION_FLAG_NONE;
        addSwapChain(pRHI->pRenderer, &swapChainDesc, &sdlWin.pSwapChain);
        ASSERT(sdlWin.pSwapChain);
    }

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

                    sdlWin.pWindow = SDL_CreateWindow(
                        e.world().has<Engine::Context>() ? e.world().get<Engine::Context>()->AppName().c_str() : APP_NAME, 
                        1920, 1080, 
                        WINDOW_FLAGS);

                    if (!sdlWin.pWindow)
                    {
                        ASSERTMSG(eERROR, "SDL failed to create window.");
                        return;
                    }
#if defined(__APPLE__)
                    sdlWin.pView = SDL_Metal_CreateView(sdlWin.pWindow);
#endif
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

                    CreateWindowSwapchain(pRHI, sdlWin, bbwidth, bbheight);

                    addSemaphore(pRHI->pRenderer, &sdlWin.pImgAcqSemaphore);
                }
            )
            .on_remove([](flecs::entity e, SDLWindow& sdlWin)
                {
                    auto pRHI = e.world().get_mut<RHI::RHI>();
                    ASSERT(pRHI);
                    
                    waitQueueIdle(pRHI->pGfxQueue);

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

                        auto pRHI = it.world().get_mut<RHI::RHI>();
                        if (pRHI)
                        {
                            waitQueueIdle(pRHI->pGfxQueue);

                            removeSwapChain(pRHI->pRenderer, sdlWin.pSwapChain);
                            sdlWin.pSwapChain = nullptr;
                            CreateWindowSwapchain(pRHI, sdlWin, bbwidth, bbheight);

                            // Update canvas size
                            canvas.width = bbwidth;
                            canvas.height = bbheight;
                        }
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
                        acquireNextImage(pRHI->pRenderer, sdlWin.pSwapChain, sdlWin.pImgAcqSemaphore, nullptr, &sdlWin.imageIndex);

                        sdlWin.pCurRT = (sdlWin.imageIndex == static_cast<unsigned int>(- 1)) ? nullptr : sdlWin.pSwapChain->ppRenderTargets[sdlWin.imageIndex];

#if DEBUG_PRESENTATION_CLEAR_COLOR_RED // Clear cur RT a red color
                        if (sdlWin.pCurRT)
                        {
                            Cmd* pCmd = pRHI->curCmdRingElem.pCmds[0];

                            RenderTargetBarrier barriers[] = {
                                { sdlWin.pCurRT, RESOURCE_STATE_PRESENT, RESOURCE_STATE_RENDER_TARGET },
                            };
                            cmdResourceBarrier(pCmd, 0, nullptr, 0, nullptr, 1, barriers);

                            BindRenderTargetsDesc bindRenderTargets = {};
                            bindRenderTargets.mRenderTargetCount = 1;
                            bindRenderTargets.mRenderTargets[0] = { sdlWin.pCurRT, LOAD_ACTION_CLEAR };
                            cmdBindRenderTargets(pCmd, &bindRenderTargets);
                            cmdSetViewport(pCmd, 0.0f, 0.0f, static_cast<float>(sdlWin.pCurRT->mWidth), static_cast<float>(sdlWin.pCurRT->mHeight), 0.0f, 1.0f);
                            cmdSetScissor(pCmd, 0, 0, sdlWin.pCurRT->mWidth, sdlWin.pCurRT->mHeight);

                            cmdBindRenderTargets(pCmd, nullptr);

                            barriers[0] = { sdlWin.pCurRT, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_PRESENT };
                            cmdResourceBarrier(pCmd, 0, nullptr, 0, nullptr, 1, barriers);
                        }
#endif
                    }
                }
            );

        auto present = ecs.system<Window::SDLWindow>("Present")
            .kind(Engine::GetCustomPhaseEntity(ecs, Engine::PRESENT))
            .each([](flecs::iter& it, size_t i, Window::SDLWindow& sdlWin)
                {
                    ASSERTMSG(i == 0, "More than one window not implemented.");

                    auto pRHI = it.world().get_mut<RHI::RHI>();
                    if (pRHI && sdlWin.pCurRT)
                    {
                        Cmd* pCmd = pRHI->curCmdRingElem.pCmds[0];
                        endCmd(pCmd);

                        FlushResourceUpdateDesc flushUpdateDesc = {};
                        flushUpdateDesc.mNodeIndex = 0;
                        flushResourceUpdates(&flushUpdateDesc);

                        Semaphore* waitSemaphores[2] = { flushUpdateDesc.pOutSubmittedSemaphore, sdlWin.pImgAcqSemaphore };

                        QueueSubmitDesc submitDesc = {};
                        submitDesc.mCmdCount = 1;
                        submitDesc.mSignalSemaphoreCount = 1;
                        submitDesc.mWaitSemaphoreCount = TF_ARRAY_COUNT(waitSemaphores);
                        submitDesc.ppCmds = &pCmd;
                        submitDesc.ppSignalSemaphores = &pRHI->curCmdRingElem.pSemaphore;
                        submitDesc.ppWaitSemaphores = waitSemaphores;
                        submitDesc.pSignalFence = pRHI->curCmdRingElem.pFence;
                        queueSubmit(pRHI->pGfxQueue, &submitDesc);

                        QueuePresentDesc presentDesc = {};
                        presentDesc.mIndex = (uint8_t)sdlWin.imageIndex;
                        presentDesc.mWaitSemaphoreCount = 1;
                        presentDesc.pSwapChain = sdlWin.pSwapChain;
                        presentDesc.ppWaitSemaphores = &pRHI->curCmdRingElem.pSemaphore;
                        presentDesc.mSubmitDone = true;

                        queuePresent(pRHI->pGfxQueue, &presentDesc);

                        pRHI->frameIndex = (pRHI->frameIndex + 1) % pRHI->dataBufferCount;

                        it.world().modified<RHI::RHI>();
                    }
                }
            );
    }
}
