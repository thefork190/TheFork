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
	};

	// A swapchain for the window
	struct Swapchain
	{
		SwapChain* pSwapChain = nullptr;
	};

	struct module
	{
		module(flecs::world& ecs); // Ctor that loads the module
	};
}
