#ifdef __ANDROID__
#include <SDL3/SDL_system.h>
#endif

#include <ILog.h>

#include "Engine.h"
#include "RHI.h"
#include "Window.h"

#define DEBUG_PRESENTATION_CLEAR_COLOR_RED 0
#if defined(__APPLE__)
#define HWND NSWindow *
#define WINDOW_PROP SDL_PROP_WINDOW_COCOA_WINDOW_POINTER
#define WINDOW_FLAGS SDL_WINDOW_RESIZABLE | SDL_WINDOW_METAL | SDL_WINDOW_HIGH_PIXEL_DENSITY
#elif defined(__ANDROID__)
#define HWND ANativeWindow*
#define WINDOW_PROP SDL_PROP_WINDOW_ANDROID_WINDOW_POINTER
#define __bridge
#define WINDOW_FLAGS SDL_WINDOW_RESIZABLE | SDL_WINDOW_VULKAN
#else
#define WINDOW_PROP SDL_PROP_WINDOW_WIN32_HWND_POINTER
#define __bridge
#define WINDOW_FLAGS SDL_WINDOW_RESIZABLE | SDL_WINDOW_VULKAN
#endif

namespace Window
{
    struct MainWindowTag {}; // Tag to easily identify the main window entity
   
    void CreateWindowSwapchain(RHI::RHI* pRHI, SDLWindow& sdlWin, int const w, int const h)
    {
        // TODO this is platform specific
        HWND pWinHandle = (__bridge HWND)SDL_GetPointerProperty(SDL_GetWindowProperties(sdlWin.pWindow), WINDOW_PROP, nullptr);
        ASSERT(pWinHandle);

        SwapChainDesc swapChainDesc = {};

        WindowHandle tfWindowHandle = {};
#if defined(__APPLE__)
        tfWindowHandle.window = sdlWin.pView;
#else
        tfWindowHandle.window = pWinHandle;
#endif

        // Need to pass extra things for Android
#ifdef __ANDROID__
        tfWindowHandle.activity = reinterpret_cast<jobject>(SDL_GetAndroidActivity());
        tfWindowHandle.jniEnv = reinterpret_cast<JNIEnv*>(SDL_GetAndroidJNIEnv());
        tfWindowHandle.type = WINDOW_HANDLE_TYPE_ANDROID;
#else
        tfWindowHandle.type = WINDOW_HANDLE_TYPE_WIN32;
#endif

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

                    if (it.world().count<MainWindowTag>() == 0)
                    {
                        // This is the 1st window created, make it the main one
                        it.entity(i).add<MainWindowTag>();
                    }
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
                    if (pRHI && sdlWin.pSwapChain)
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
            .each([](flecs::iter& it, size_t i, Window::SDLWindow const& sdlWin)
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

    void module::ProcessEvent(flecs::world& ecs, const SDL_Event* sdlEvent)
    {
        // Can't do anything without the RHI
        auto pRHI = ecs.has<RHI::RHI>() ? ecs.get_mut<RHI::RHI>() : nullptr;
        if (!pRHI)
            return;

        switch (sdlEvent->type)
        {
            case SDL_EVENT_WILL_ENTER_BACKGROUND:
            case SDL_EVENT_WINDOW_HIDDEN:
            case SDL_EVENT_WINDOW_MINIMIZED:
            {
                // Need to remove all swapchains
                flecs::query<Window::SDLWindow> windowQuery = ecs.query_builder<Window::SDLWindow>().build();
                windowQuery.each([pRHI](flecs::iter& it, size_t i, Window::SDLWindow& sdlWin)
                    {
                        if (sdlWin.pSwapChain)
                        {
                            waitQueueIdle(pRHI->pGfxQueue);
                            removeSwapChain(pRHI->pRenderer, sdlWin.pSwapChain);
                            sdlWin.pSwapChain = nullptr;
                            sdlWin.pCurRT = nullptr;
                        }
                    });
                break;
            }
            case SDL_EVENT_DID_ENTER_FOREGROUND:
            case SDL_EVENT_WINDOW_RESTORED:
            {
                // Need to recreate all swapchains
                flecs::query<Window::SDLWindow> windowQuery = ecs.query_builder<Window::SDLWindow>().build();
                windowQuery.each([pRHI](flecs::iter& it, size_t i, Window::SDLWindow& sdlWin)
                    {
                        if (!sdlWin.pSwapChain)
                        {
                            waitQueueIdle(pRHI->pGfxQueue);
                            int bbwidth, bbheight;
                            SDL_GetWindowSizeInPixels(sdlWin.pWindow, &bbwidth, &bbheight);
                            CreateWindowSwapchain(pRHI, sdlWin, bbwidth, bbheight);
                        }
                    });
                break;
            }
        }
    }

    bool MainWindow(flecs::world& ecs, SDLWindow const** ppMainWindowOut)
    {
        // Usually, the main window will be needed to initialize things (eg. rendering related).
        // It might also be needed when resizing things, but it's queried outside of systems pretty sparringly.
        // If this changes, then the query should be cached.
        flecs::query<SDLWindow, MainWindowTag> mainWindowQuery = ecs.query_builder<SDLWindow, MainWindowTag>().build();

        uint32_t mainWindowsFound = 0;
        mainWindowQuery.each([&ppMainWindowOut, &mainWindowsFound](flecs::iter& it, size_t i, SDLWindow const& window, MainWindowTag)
            {
                mainWindowsFound++;
                if (ppMainWindowOut && mainWindowsFound == 1)
                    *ppMainWindowOut = &window;
            });

        ASSERT(mainWindowsFound == 1);

        return mainWindowsFound == 1;
    }
}
