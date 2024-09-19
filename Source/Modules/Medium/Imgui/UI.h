#pragma once

#include <flecs.h>
#include "LifeCycledModule.h"

namespace UI
{
	struct UI
	{
		// TODO: callback where we pass in ecs world
	};

	class module : public LifeCycledModule
	{
	public:
		module(flecs::world& ecs); // Ctor that loads the module
		virtual void OnExit(flecs::world& ecs) override;
	};

	void ForwardEvent(flecs::world& ecs, const SDL_Event* sdlEvent);
}
