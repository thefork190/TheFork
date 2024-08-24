#pragma once

#include <SDL3/SDL.h>
#include <flecs.h>
#include <IGraphics.h>
#include <RingBuffer.h>

namespace Window
{
	// An SDL window
	struct SDLWindow
	{
		SDL_Window* pWindow = nullptr;
		SwapChain* pSwapChain = nullptr;
		Semaphore* pImgAcqSemaphore = nullptr;
		unsigned int imageIndex = 0;
		RenderTarget* pCurRT = nullptr;
		GpuCmdRingElement curCmdRingElem = {};
	};

	struct module
	{
		module(flecs::world& ecs); // Ctor that loads the module
	};
}
