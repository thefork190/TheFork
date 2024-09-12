#pragma once

#include <SDL3/SDL.h>
#include <flecs.h>
#include <IGraphics.h>
#include <RingBuffer.h>
#include "LifeCycledModule.h"

namespace Window
{
	// An SDL window
	struct SDLWindow
	{
		SDL_Window* pWindow = nullptr;
#if defined(__APPLE__)
        SDL_MetalView pView = nullptr;
#endif
		SwapChain* pSwapChain = nullptr;
		Semaphore* pImgAcqSemaphore = nullptr;
		unsigned int imageIndex = 0;
		RenderTarget* pCurRT = nullptr;
	};

	class module : public LifeCycledModule
	{
	public:
		module(flecs::world& ecs); // Ctor that loads the module
	};
}
