#pragma once

#include <SDL3/SDL.h>
#include <flecs.h>

namespace Window
{
	// An SDL window
	struct SDLWindow
	{
		SDL_Window* pWindow = nullptr;
	};

	struct module
	{
		module(flecs::world& ecs); // Ctor that loads the module
	};
}
