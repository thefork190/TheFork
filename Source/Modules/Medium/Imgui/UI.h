#pragma once

#include <functional>
#include <flecs.h>
#include "LifeCycledModule.h"

namespace UI
{
	struct UI
	{
		std::function<void(flecs::world&)> Update;
	};

	class module : public LifeCycledModule
	{
	public:
		module(flecs::world& ecs); // Ctor that loads the module
		virtual void OnExit(flecs::world& ecs) override;
	};

	// Forwards SDL events
	void ForwardEvent(flecs::world& ecs, const SDL_Event* sdlEvent);
	
	// Checks if UI is currently capturing inputs (in which case the app probably shouldn't handle inputs)
	bool WantsCaptureInputs(flecs::world& ecs);
}
