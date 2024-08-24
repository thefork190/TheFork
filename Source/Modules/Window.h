#pragma once

#include <SDL3/SDL.h>
#include <flecs.h>
#include <IGraphics.h>

namespace Window
{
	// An SDL window
	struct SDLWindow
	{
		SDL_Window* pWindow = nullptr;
		SwapChain* pSwapChain = nullptr;
		Semaphore* pImgAcqSemaphore = nullptr;
	};

	struct module
	{
		module(flecs::world& ecs); // Ctor that loads the module
	};
}
